#ifdef _WIN32
#include "media_stream.hpp"
#include "media_source.hpp"
#include "../vcam_shared_mem.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

HRESULT MtgVCamStream::Create(MtgVCamSource* source,
                               IMFStreamDescriptor* sd,
                               MtgVCamStream** ppStream) {
    if (!ppStream) return E_POINTER;
    auto* s = new (std::nothrow) MtgVCamStream();
    if (!s) return E_OUTOFMEMORY;
    HRESULT hr = s->Init(source, sd);
    if (FAILED(hr)) { s->Release(); return hr; }
    *ppStream = s;
    return S_OK;
}

HRESULT MtgVCamStream::Init(MtgVCamSource* source, IMFStreamDescriptor* sd) {
    InitializeCriticalSection(&cs_);
    source_      = source;
    stream_desc_ = sd;

    HRESULT hr = MFCreateEventQueue(&event_queue_);
    if (FAILED(hr)) return hr;

    // Open the shared memory written by the main app.
    // If the app isn't running, shm_view_ stays null and we deliver black frames.
    shm_handle_ = OpenFileMappingW(FILE_MAP_READ, FALSE, VCAM_SHM_NAME);
    if (shm_handle_) {
        shm_view_ = MapViewOfFile(shm_handle_, FILE_MAP_READ, 0, 0, VCAM_SHM_SIZE);
    }
    mutex_handle_ = OpenMutexW(SYNCHRONIZE, FALSE, VCAM_MUTEX_NAME);

    return S_OK;
}

MtgVCamStream::~MtgVCamStream() {
    if (shm_view_)    UnmapViewOfFile(shm_view_);
    if (shm_handle_)  CloseHandle(shm_handle_);
    if (mutex_handle_)CloseHandle(mutex_handle_);
    DeleteCriticalSection(&cs_);
}

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamStream::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IMFMediaEventGenerator || riid == IID_IMFMediaStream) {
        *ppv = static_cast<IMFMediaStream*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) MtgVCamStream::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) MtgVCamStream::Release() {
    ULONG n = InterlockedDecrement(&ref_count_);
    if (n == 0) delete this;
    return n;
}

// ---------------------------------------------------------------------------
// IMFMediaEventGenerator
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamStream::GetEvent(DWORD flags, IMFMediaEvent** ppEvent) {
    return event_queue_->GetEvent(flags, ppEvent);
}
IFACEMETHODIMP MtgVCamStream::BeginGetEvent(IMFAsyncCallback* cb, IUnknown* state) {
    return event_queue_->BeginGetEvent(cb, state);
}
IFACEMETHODIMP MtgVCamStream::EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** ppEvent) {
    return event_queue_->EndGetEvent(result, ppEvent);
}
IFACEMETHODIMP MtgVCamStream::QueueEvent(MediaEventType type, REFGUID ext,
                                          HRESULT status, const PROPVARIANT* val) {
    return event_queue_->QueueEventParamVar(type, ext, status, val);
}

// ---------------------------------------------------------------------------
// IMFMediaStream
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamStream::GetMediaSource(IMFMediaSource** ppSource) {
    if (!ppSource) return E_POINTER;
    EnterCriticalSection(&cs_);
    HRESULT hr = shutdown_ ? MF_E_SHUTDOWN : S_OK;
    if (SUCCEEDED(hr)) {
        *ppSource = static_cast<IMFMediaSource*>(source_);
        (*ppSource)->AddRef();
    }
    LeaveCriticalSection(&cs_);
    return hr;
}

IFACEMETHODIMP MtgVCamStream::GetStreamDescriptor(IMFStreamDescriptor** ppSD) {
    if (!ppSD) return E_POINTER;
    EnterCriticalSection(&cs_);
    HRESULT hr = shutdown_ ? MF_E_SHUTDOWN : S_OK;
    if (SUCCEEDED(hr)) {
        *ppSD = stream_desc_.Get();
        (*ppSD)->AddRef();
    }
    LeaveCriticalSection(&cs_);
    return hr;
}

IFACEMETHODIMP MtgVCamStream::RequestSample(IUnknown* token) {
    EnterCriticalSection(&cs_);
    if (shutdown_) { LeaveCriticalSection(&cs_); return MF_E_SHUTDOWN; }
    if (!active_)  { LeaveCriticalSection(&cs_); return MF_E_INVALIDREQUEST; }
    LeaveCriticalSection(&cs_);

    // Try to get a frame from shared memory
    if (shm_view_ && mutex_handle_) {
        DWORD wait = WaitForSingleObject(mutex_handle_, 8);
        if (wait == WAIT_OBJECT_0) {
            const auto* shm = static_cast<const VcamSharedMem*>(shm_view_);
            uint32_t w = shm->width, h = shm->height;
            uint32_t fc = shm->frame_count;

            if (w > 0 && h > 0 && w <= VCAM_SHM_MAX_W && h <= VCAM_SHM_MAX_H) {
                // Build an MF sample from the BGRA pixels
                ComPtr<IMFSample> sample;
                ComPtr<IMFMediaBuffer> buf;
                DWORD bufSize = w * h * 4;

                HRESULT hr = MFCreateSample(&sample);
                if (SUCCEEDED(hr)) hr = MFCreateMemoryBuffer(bufSize, &buf);
                if (SUCCEEDED(hr)) {
                    BYTE* pData = nullptr;
                    hr = buf->Lock(&pData, nullptr, nullptr);
                    if (SUCCEEDED(hr)) {
                        std::memcpy(pData, shm->bgra,
                            std::min<std::size_t>(bufSize, sizeof(shm->bgra)));
                        buf->Unlock();
                        buf->SetCurrentLength(bufSize);
                    }
                }
                if (SUCCEEDED(hr)) {
                    sample->AddBuffer(buf.Get());
                    sample->SetSampleTime(MFGetSystemTime());
                    sample->SetSampleDuration(333333LL); // ~30fps in 100-ns units
                    if (token) sample->SetUnknown(MFSampleExtension_Token, token);
                    last_frame_count_ = fc;
                }
                ReleaseMutex(mutex_handle_);

                if (SUCCEEDED(hr)) {
                    return event_queue_->QueueEventParamUnk(
                        MEMediaSample, GUID_NULL, S_OK, sample.Get());
                }
            } else {
                ReleaseMutex(mutex_handle_);
            }
        }
    }

    // Fallback: deliver a black frame so the pipeline doesn't stall
    return DeliverBlackFrame(token);
}

HRESULT MtgVCamStream::DeliverBlackFrame(IUnknown* token) {
    // Use the last known resolution, or a safe default
    uint32_t w = 1280, h = 800;
    if (shm_view_) {
        const auto* shm = static_cast<const VcamSharedMem*>(shm_view_);
        if (shm->width > 0 && shm->width <= VCAM_SHM_MAX_W)  w = shm->width;
        if (shm->height > 0 && shm->height <= VCAM_SHM_MAX_H) h = shm->height;
    }

    ComPtr<IMFSample>      sample;
    ComPtr<IMFMediaBuffer> buf;
    HRESULT hr = MFCreateSample(&sample);
    if (SUCCEEDED(hr)) hr = MFCreateMemoryBuffer(w * h * 4, &buf);
    if (SUCCEEDED(hr)) {
        BYTE* pData = nullptr;
        hr = buf->Lock(&pData, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            std::memset(pData, 0, w * h * 4);
            buf->Unlock();
            buf->SetCurrentLength(w * h * 4);
        }
    }
    if (SUCCEEDED(hr)) {
        sample->AddBuffer(buf.Get());
        sample->SetSampleTime(MFGetSystemTime());
        sample->SetSampleDuration(333333LL);
        if (token) sample->SetUnknown(MFSampleExtension_Token, token);
        hr = event_queue_->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, sample.Get());
    }
    return hr;
}

// ---------------------------------------------------------------------------
// State transitions (called by MtgVCamSource)
// ---------------------------------------------------------------------------

HRESULT MtgVCamStream::Start() {
    EnterCriticalSection(&cs_);
    active_ = true;
    LeaveCriticalSection(&cs_);
    return event_queue_->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, nullptr);
}

HRESULT MtgVCamStream::Stop() {
    EnterCriticalSection(&cs_);
    active_ = false;
    LeaveCriticalSection(&cs_);
    return event_queue_->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr);
}

HRESULT MtgVCamStream::Shutdown() {
    EnterCriticalSection(&cs_);
    shutdown_ = true;
    active_   = false;
    LeaveCriticalSection(&cs_);
    return event_queue_->Shutdown();
}

#endif // _WIN32
