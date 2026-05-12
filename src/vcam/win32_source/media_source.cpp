#ifdef _WIN32
#include "media_source.hpp"
#include "media_stream.hpp"
#include "../vcam_shared_mem.hpp"
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <cstdio>

static void SrcLog(const char* msg) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\ProgramData\\mtg-vcam-debug.log", "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

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

// {FB6C4281-0353-11d1-905F-0000C0CC16BA} = PINNAME_VIDEO_CAPTURE
static const GUID kPinNameVideoCapture =
    {0xFB6C4281, 0x0353, 0x11d1, {0x90, 0x5F, 0x00, 0x00, 0xC0, 0xCC, 0x16, 0xBA}};

HRESULT MtgVCamSource::Init() {
    SrcLog("MtgVCamSource::Init start");
    InitializeCriticalSection(&cs_);

    HRESULT hr = MFCreateEventQueue(&event_queue_);
    if (FAILED(hr)) { char b[64]; sprintf_s(b,"MFCreateEventQueue failed 0x%08X",(unsigned)hr); SrcLog(b); return hr; }
    SrcLog("MFCreateEventQueue OK");

    // Source attributes — identify this as a video capture source
    hr = MFCreateAttributes(&source_attribs_, 4);
    if (FAILED(hr)) { char b[64]; sprintf_s(b,"MFCreateAttributes failed 0x%08X",(unsigned)hr); SrcLog(b); return hr; }
    source_attribs_->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                              MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    // Stream attributes — required by Frame Server to share the stream
    hr = MFCreateAttributes(&stream_attribs_, 4);
    if (FAILED(hr)) return hr;
    stream_attribs_->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, kPinNameVideoCapture);
    stream_attribs_->SetUINT32(MF_DEVICESTREAM_STREAM_ID, 0);
    stream_attribs_->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1);

    ComPtr<IMFStreamDescriptor> sd;
    hr = BuildStreamDescriptor(&sd);
    if (FAILED(hr)) { char b[64]; sprintf_s(b,"BuildStreamDescriptor failed 0x%08X",(unsigned)hr); SrcLog(b); return hr; }

    // Mirror the stream attrs onto the descriptor itself (Frame Server reads both)
    sd->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, kPinNameVideoCapture);
    sd->SetUINT32(MF_DEVICESTREAM_STREAM_ID, 0);
    sd->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1);

    IMFStreamDescriptor* sdRaw = sd.Get();
    hr = MFCreatePresentationDescriptor(1, &sdRaw, &pd_);
    if (FAILED(hr)) { char b[64]; sprintf_s(b,"MFCreatePresentationDescriptor failed 0x%08X",(unsigned)hr); SrcLog(b); return hr; }
    pd_->SelectStream(0);

    hr = MtgVCamStream::Create(this, sd.Get(), &stream_);
    char b[64]; sprintf_s(b,"MtgVCamStream::Create hr=0x%08X",(unsigned)hr); SrcLog(b);
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
        riid == IID_IMFMediaSource || riid == __uuidof(IMFMediaSourceEx)) {
        *ppv = static_cast<IMFMediaSourceEx*>(this);
    } else if (riid == __uuidof(IKsControl)) {
        *ppv = static_cast<IKsControl*>(this);
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
    SrcLog("IMFMediaSource::Start called");
    EnterCriticalSection(&cs_);
    if (shutdown_) { LeaveCriticalSection(&cs_); return MF_E_SHUTDOWN; }
    LeaveCriticalSection(&cs_);

    HRESULT hr = stream_->Start();
    char b2[64]; sprintf_s(b2, "stream Start hr=0x%08X", (unsigned)hr); SrcLog(b2);
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

IFACEMETHODIMP MtgVCamSource::GetStreamAttributes(DWORD dwStreamIdentifier,
                                                    IMFAttributes** ppAttribs) {
    if (!ppAttribs) return E_POINTER;
    if (dwStreamIdentifier != 0) return MF_E_INVALIDSTREAMNUMBER;
    *ppAttribs = stream_attribs_.Get();
    (*ppAttribs)->AddRef();
    return S_OK;
}

IFACEMETHODIMP MtgVCamSource::SetD3DManager(IUnknown* /*pManager*/) {
    return S_OK; // GPU conversion not implemented; accept call gracefully
}

// ---------------------------------------------------------------------------
// IKsControl — no camera controls; just satisfy the Frame Server's QI
// ---------------------------------------------------------------------------

STDMETHODIMP MtgVCamSource::KsProperty(void*, ULONG, void*, ULONG, ULONG* BytesReturned) {
    if (BytesReturned) *BytesReturned = 0;
    return E_NOTIMPL;
}
STDMETHODIMP MtgVCamSource::KsMethod(void*, ULONG, void*, ULONG, ULONG* BytesReturned) {
    if (BytesReturned) *BytesReturned = 0;
    return E_NOTIMPL;
}
STDMETHODIMP MtgVCamSource::KsEvent(void*, ULONG, void*, ULONG, ULONG* BytesReturned) {
    if (BytesReturned) *BytesReturned = 0;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// MtgVCamActivate
// ---------------------------------------------------------------------------

HRESULT MtgVCamActivate::Create(MtgVCamActivate** pp) {
    if (!pp) return E_POINTER;
    auto* a = new (std::nothrow) MtgVCamActivate();
    if (!a) return E_OUTOFMEMORY;
    HRESULT hr = MFCreateAttributes(&a->attrs_, 4);
    if (FAILED(hr)) { delete a; return hr; }
    *pp = a;
    return S_OK;
}

IFACEMETHODIMP MtgVCamActivate::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IMFAttributes || riid == __uuidof(IMFActivate))
        *ppv = static_cast<IMFActivate*>(this);
    else { *ppv = nullptr; return E_NOINTERFACE; }
    AddRef();
    return S_OK;
}
IFACEMETHODIMP_(ULONG) MtgVCamActivate::AddRef() { return InterlockedIncrement(&ref_); }
IFACEMETHODIMP_(ULONG) MtgVCamActivate::Release() {
    ULONG n = InterlockedDecrement(&ref_);
    if (n == 0) delete this;
    return n;
}

IFACEMETHODIMP MtgVCamActivate::ActivateObject(REFIID riid, void** ppv) {
    SrcLog("IMFActivate::ActivateObject called");
    if (!source_) {
        HRESULT hr = MtgVCamSource::Create(&source_);
        if (FAILED(hr)) {
            char b[64]; sprintf_s(b, "ActivateObject Create failed 0x%08X", (unsigned)hr);
            SrcLog(b);
            return hr;
        }
        SrcLog("MtgVCamSource created OK");
    }
    return source_->QueryInterface(riid, ppv);
}

IFACEMETHODIMP MtgVCamActivate::ShutdownObject() {
    SrcLog("IMFActivate::ShutdownObject called");
    if (source_) { source_->Shutdown(); source_->Release(); source_ = nullptr; }
    return S_OK;
}

IFACEMETHODIMP MtgVCamActivate::DetachObject() {
    if (source_) { source_->Release(); source_ = nullptr; }
    return S_OK;
}

#endif // _WIN32
