#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <ks.h>
#include <ksmedia.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class MtgVCamStream;

// IMFMediaSource + IMFMediaSourceEx + IKsControl + IMFGetService
class MtgVCamSource final
    : public IMFMediaSource
    , public IMFMediaSourceEx
    , public IKsControl
    , public IMFGetService {
public:
    static HRESULT Create(MtgVCamSource** ppSource);

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

    // IMFMediaSource
    IFACEMETHODIMP GetCharacteristics(DWORD* pdwCharacteristics) override;
    IFACEMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** ppPD) override;
    IFACEMETHODIMP Start(IMFPresentationDescriptor* pPD,
                         const GUID* pguidTimeFormat,
                         const PROPVARIANT* pvarStartPos) override;
    IFACEMETHODIMP Stop() override;
    IFACEMETHODIMP Pause() override;
    IFACEMETHODIMP Shutdown() override;

    // IMFMediaSourceEx
    IFACEMETHODIMP GetSourceAttributes(IMFAttributes** ppAttribs) override;
    IFACEMETHODIMP GetStreamAttributes(DWORD dwStreamIdentifier,
                                       IMFAttributes** ppAttribs) override;
    IFACEMETHODIMP SetD3DManager(IUnknown* pManager) override;

    // IKsControl
    IFACEMETHODIMP KsProperty(PKSPROPERTY prop, ULONG propLen,
                              LPVOID propData, ULONG dataLen, ULONG* bytesRet) override;
    IFACEMETHODIMP KsMethod(PKSMETHOD method, ULONG methodLen,
                            LPVOID methodData, ULONG dataLen, ULONG* bytesRet) override;
    IFACEMETHODIMP KsEvent(PKSEVENT evt, ULONG evtLen,
                           LPVOID evtData, ULONG dataLen, ULONG* bytesRet) override;

    // IMFGetService
    IFACEMETHODIMP GetService(REFGUID guidService, REFIID riid, LPVOID* ppvObject) override;

private:
    MtgVCamSource() = default;
    ~MtgVCamSource();

    HRESULT Init();
    HRESULT BuildStreamDescriptor(IMFStreamDescriptor** ppSD);

    volatile LONG              ref_count_ = 1;
    ComPtr<IMFMediaEventQueue> event_queue_;
    ComPtr<IMFPresentationDescriptor> pd_;
    ComPtr<IMFAttributes>      source_attribs_;
    MtgVCamStream*             stream_    = nullptr;
    bool                       shutdown_  = false;
    CRITICAL_SECTION           cs_;
};

#endif // _WIN32
