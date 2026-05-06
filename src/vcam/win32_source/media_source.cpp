#ifdef _WIN32
#include "media_source.hpp"
#include "media_stream.hpp"
#include "../vcam_shared_mem.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

HRESULT MtgVCamSource::Create(MtgVCamSource** ppSource) {
    if (!ppSource) return E_POINTER;
    auto* s = new (std::nothrow) MtgVCamSource();
    if (!s) return E_OUTOFMEMORY;
    HRESULT hr = s->Init();
    if (FAILED(hr)) { s->Release(); return hr; }
    *ppSource = s;
    return S_OK;
}

HRESULT MtgVCamSource::Init() {
    InitializeCriticalSection(&cs_);

    HRESULT hr = MFCreateEventQueue(&event_queue_);
    if (FAILED(hr)) return hr;

    hr = MFCreateAttributes(&source_attribs_, 4);
    if (FAILED(hr)) return hr;

    // Build the stream descriptor and presentation descriptor
    ComPtr<IMFStreamDescriptor> sd;
    hr = BuildStreamDescriptor(&sd);
    if (FAILED(hr)) return hr;

    IMFStreamDescriptor* sdRaw = sd.Get();
    hr = MFCreatePresentationDescriptor(1, &sdRaw, &pd_);
    if (FAILED(hr)) return hr;
    pd_->SelectStream(0);

    hr = MtgVCamStream::Create(this, sd.Get(), &stream_);
    return hr;
}

MtgVCamSource::~MtgVCamSource() {
    if (stream_) stream_->Release();
    DeleteCriticalSection(&cs_);
}

// Build a stream descriptor offering ARGB32 at common resolutions.
// The Frame Server / client negotiates which format to use.
HRESULT MtgVCamSource::BuildStreamDescriptor(IMFStreamDescriptor** ppSD) {
    // Offer a handful of resolutions at 30fps, all in ARGB32 (top-down BGRA bytes).
    struct Res { UINT32 w, h; };
    static const Res kSizes[] = {
        {1280, 800}, {1280, 720}, {1920, 1080}, {640, 480},
    };
    const int nTypes = ARRAYSIZE(kSizes);

    IMFMediaType** types = new (std::nothrow) IMFMediaType*[nTypes]();
    if (!types) return E_OUTOFMEMORY;

    HRESULT hr = S_OK;
    for (int i = 0; i < nTypes && SUCCEEDED(hr); ++i) {
        hr = MFCreateMediaType(&types[i]);
        if (FAILED(hr)) break;
        types[i]->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        types[i]->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);
        MFSetAttributeSize(types[i], MF_MT_FRAME_SIZE, kSizes[i].w, kSizes[i].h);
        MFSetAttributeRatio(types[i], MF_MT_FRAME_RATE, 30, 1);
        MFSetAttributeRatio(types[i], MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        types[i]->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        // Positive stride = top-down storage
        types[i]->SetUINT32(MF_MT_DEFAULT_STRIDE, kSizes[i].w * 4);
        types[i]->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    }

    IMFStreamDescriptor* sd = nullptr;
    if (SUCCEEDED(hr))
        hr = MFCreateStreamDescriptor(0, nTypes, types, &sd);

    if (SUCCEEDED(hr)) {
        // Set default media type to first entry
        IMFMediaTypeHandler* handler = nullptr;
        sd->GetMediaTypeHandler(&handler);
        if (handler) {
            handler->SetCurrentMediaType(types[0]);
            handler->Release();
        }
        *ppSD = sd;
    }

    for (int i = 0; i < nTypes; ++i)
        if (types[i]) types[i]->Release();
    delete[] types;
    return hr;
}

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamSource::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IMFMediaEventGenerator ||
        riid == IID_IMFMediaSource) {
        *ppv = static_cast<IMFMediaSource*>(this);
    } else if (riid == __uuidof(IMFMediaSourceEx)) {
        *ppv = static_cast<IMFMediaSourceEx*>(this);
    } else if (riid == __uuidof(IKsControl)) {
        *ppv = static_cast<IKsControl*>(this);
    } else if (riid == __uuidof(IMFGetService)) {
        *ppv = static_cast<IMFGetService*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) MtgVCamSource::AddRef() {
    return InterlockedIncrement(&ref_count_);
}
IFACEMETHODIMP_(ULONG) MtgVCamSource::Release() {
    ULONG n = InterlockedDecrement(&ref_count_);
    if (n == 0) delete this;
    return n;
}

// ---------------------------------------------------------------------------
// IMFMediaEventGenerator
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamSource::GetEvent(DWORD flags, IMFMediaEvent** ppEvent) {
    return event_queue_->GetEvent(flags, ppEvent);
}
IFACEMETHODIMP MtgVCamSource::BeginGetEvent(IMFAsyncCallback* cb, IUnknown* state) {
    return event_queue_->BeginGetEvent(cb, state);
}
IFACEMETHODIMP MtgVCamSource::EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** ppEvent) {
    return event_queue_->EndGetEvent(result, ppEvent);
}
IFACEMETHODIMP MtgVCamSource::QueueEvent(MediaEventType type, REFGUID ext,
                                          HRESULT status, const PROPVARIANT* val) {
    return event_queue_->QueueEventParamVar(type, ext, status, val);
}

// ---------------------------------------------------------------------------
// IMFMediaSource
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamSource::GetCharacteristics(DWORD* pdw) {
    if (!pdw) return E_POINTER;
    // Live source: cannot seek, cannot pause
    *pdw = MFMEDIASOURCE_IS_LIVE;
    return S_OK;
}

IFACEMETHODIMP MtgVCamSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPD) {
    if (!ppPD) return E_POINTER;
    EnterCriticalSection(&cs_);
    HRESULT hr = shutdown_ ? MF_E_SHUTDOWN : pd_->Clone(ppPD);
    LeaveCriticalSection(&cs_);
    return hr;
}

IFACEMETHODIMP MtgVCamSource::Start(IMFPresentationDescriptor* /*pPD*/,
                                     const GUID* /*pguidTimeFormat*/,
                                     const PROPVARIANT* /*pvarStartPos*/) {
    EnterCriticalSection(&cs_);
    if (shutdown_) { LeaveCriticalSection(&cs_); return MF_E_SHUTDOWN; }
    LeaveCriticalSection(&cs_);

    HRESULT hr = stream_->Start();
    if (SUCCEEDED(hr))
        hr = event_queue_->QueueEventParamVar(MESourceStarted, GUID_NULL, S_OK, nullptr);
    return hr;
}

IFACEMETHODIMP MtgVCamSource::Stop() {
    EnterCriticalSection(&cs_);
    if (shutdown_) { LeaveCriticalSection(&cs_); return MF_E_SHUTDOWN; }
    LeaveCriticalSection(&cs_);

    HRESULT hr = stream_->Stop();
    if (SUCCEEDED(hr))
        hr = event_queue_->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, nullptr);
    return hr;
}

IFACEMETHODIMP MtgVCamSource::Pause() {
    // Live sources cannot pause
    return MF_E_INVALID_STATE_TRANSITION;
}

IFACEMETHODIMP MtgVCamSource::Shutdown() {
    EnterCriticalSection(&cs_);
    if (shutdown_) { LeaveCriticalSection(&cs_); return MF_E_SHUTDOWN; }
    shutdown_ = true;
    LeaveCriticalSection(&cs_);

    stream_->Shutdown();
    event_queue_->Shutdown();
    return S_OK;
}

// ---------------------------------------------------------------------------
// IMFMediaSourceEx
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamSource::GetSourceAttributes(IMFAttributes** ppAttribs) {
    if (!ppAttribs) return E_POINTER;
    *ppAttribs = source_attribs_.Get();
    (*ppAttribs)->AddRef();
    return S_OK;
}

IFACEMETHODIMP MtgVCamSource::GetStreamAttributes(DWORD /*dwStreamIdentifier*/,
                                                    IMFAttributes** ppAttribs) {
    if (!ppAttribs) return E_POINTER;
    // Return empty attributes for the stream
    return MFCreateAttributes(ppAttribs, 0);
}

IFACEMETHODIMP MtgVCamSource::SetD3DManager(IUnknown* /*pManager*/) {
    return S_OK; // GPU conversion not implemented; accept call gracefully
}

// ---------------------------------------------------------------------------
// IKsControl  (camera property / control interface — required by Frame Server)
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamSource::KsProperty(PKSPROPERTY prop, ULONG propLen,
                                           LPVOID propData, ULONG dataLen, ULONG* bytesRet) {
    // Return KSPROPERTY_TYPE_GET for the stream category so the camera appears
    // in the system. Everything else is not supported.
    if (bytesRet) *bytesRet = 0;
    return E_NOTIMPL;
}

IFACEMETHODIMP MtgVCamSource::KsMethod(PKSMETHOD, ULONG, LPVOID, ULONG, ULONG* br) {
    if (br) *br = 0;
    return E_NOTIMPL;
}

IFACEMETHODIMP MtgVCamSource::KsEvent(PKSEVENT, ULONG, LPVOID, ULONG, ULONG* br) {
    if (br) *br = 0;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// IMFGetService
// ---------------------------------------------------------------------------

IFACEMETHODIMP MtgVCamSource::GetService(REFGUID /*guidService*/, REFIID /*riid*/,
                                          LPVOID* ppvObject) {
    if (ppvObject) *ppvObject = nullptr;
    return MF_E_UNSUPPORTED_SERVICE;
}

#endif // _WIN32
