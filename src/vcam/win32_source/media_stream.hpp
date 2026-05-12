#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

class MtgVCamSource;

class MtgVCamStream final : public IMFMediaStream {
public:
    static HRESULT Create(MtgVCamSource* source,
                          IMFStreamDescriptor* sd,
                          MtgVCamStream** ppStream);

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IMFMediaEventGenerator
    IFACEMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** ppEvent) override;
    IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* cb, IUnknown* state) override;
    IFACEMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** ppEvent) override;
    IFACEMETHODIMP QueueEvent(MediaEventType type, REFGUID ext,
                              HRESULT status, const PROPVARIANT* val) override;

    // IMFMediaStream
    IFACEMETHODIMP GetMediaSource(IMFMediaSource** ppSource) override;
    IFACEMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** ppSD) override;
    IFACEMETHODIMP RequestSample(IUnknown* token) override;

    // Called by MtgVCamSource to signal state changes
    HRESULT Start();
    HRESULT Stop();
    HRESULT Shutdown();

private:
    MtgVCamStream() = default;
    ~MtgVCamStream();

    HRESULT Init(MtgVCamSource* source, IMFStreamDescriptor* sd);
    HRESULT DeliverBlackFrame(IUnknown* token);

    volatile LONG             ref_count_  = 1;
    ComPtr<IMFMediaEventQueue> event_queue_;
    ComPtr<IMFStreamDescriptor> stream_desc_;
    MtgVCamSource*            source_     = nullptr;

    // Shared memory handles (opened in Init, closed in destructor)
    HANDLE shm_handle_   = nullptr;
    HANDLE mutex_handle_ = nullptr;
    void*  shm_view_     = nullptr;

    uint32_t last_frame_count_ = 0;
    LONG     sample_count_     = 0;
    bool     active_           = false;
    bool     shutdown_         = false;
    CRITICAL_SECTION cs_;
};

#endif // _WIN32
