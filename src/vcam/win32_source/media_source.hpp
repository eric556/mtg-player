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

using Microsoft::WRL::ComPtr;

// IKsControl {28F54685-06FD-11D2-B27A-00A0C9223196}
// Define inline to avoid ks.h's NTSTATUS return-type mismatch.
// The Frame Server queries for this interface; vtable layout matches at the binary level.
#ifndef __IKsControl_FWD_DEFINED__
#define __IKsControl_FWD_DEFINED__
MIDL_INTERFACE("28F54685-06FD-11D2-B27A-00A0C9223196")
IKsControl : public IUnknown {
    STDMETHOD(KsProperty)(void* Property, ULONG PropertyLength,
                          void* PropertyData, ULONG DataLength,
                          ULONG* BytesReturned) PURE;
    STDMETHOD(KsMethod)(void* Method, ULONG MethodLength,
                        void* MethodData, ULONG DataLength,
                        ULONG* BytesReturned) PURE;
    STDMETHOD(KsEvent)(void* Event, ULONG EventLength,
                       void* EventData, ULONG DataLength,
                       ULONG* BytesReturned) PURE;
};
#endif

class MtgVCamStream;

// IMFMediaSourceEx already inherits IMFMediaSource; do not list both.
// IKsControl is required by the Frame Server when activating AllUsers virtual cameras.
class MtgVCamSource final
    : public IMFMediaSourceEx
    , public IKsControl {
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

    // IKsControl — required by Frame Server; no camera controls implemented
    STDMETHOD(KsProperty)(void* Property, ULONG PropertyLength,
                          void* PropertyData, ULONG DataLength,
                          ULONG* BytesReturned) override;
    STDMETHOD(KsMethod)(void* Method, ULONG MethodLength,
                        void* MethodData, ULONG DataLength,
                        ULONG* BytesReturned) override;
    STDMETHOD(KsEvent)(void* Event, ULONG EventLength,
                       void* EventData, ULONG DataLength,
                       ULONG* BytesReturned) override;

private:
    MtgVCamSource() = default;
    ~MtgVCamSource();

    HRESULT Init();
    HRESULT BuildStreamDescriptor(IMFStreamDescriptor** ppSD);

    volatile LONG              ref_count_ = 1;
    ComPtr<IMFMediaEventQueue> event_queue_;
    ComPtr<IMFPresentationDescriptor> pd_;
    ComPtr<IMFAttributes>      source_attribs_;
    ComPtr<IMFAttributes>      stream_attribs_;
    MtgVCamStream*             stream_    = nullptr;
    bool                       shutdown_  = false;
    CRITICAL_SECTION           cs_;
};

// ---------------------------------------------------------------------------
// MtgVCamActivate
// The Frame Server calls CoCreateInstance asking for IMFActivate, then calls
// ActivateObject() to lazily create the real IMFMediaSource.
// IMFActivate inherits IMFAttributes; all attribute methods delegate to attrs_.
// ---------------------------------------------------------------------------
class MtgVCamActivate final : public IMFActivate {
public:
    static HRESULT Create(MtgVCamActivate** pp);

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IMFAttributes — all delegate to attrs_
    IFACEMETHODIMP GetItem(REFGUID g, PROPVARIANT* pv) override               { return attrs_->GetItem(g,pv); }
    IFACEMETHODIMP GetItemType(REFGUID g, MF_ATTRIBUTE_TYPE* t) override      { return attrs_->GetItemType(g,t); }
    IFACEMETHODIMP CompareItem(REFGUID g, REFPROPVARIANT v, BOOL* pb) override { return attrs_->CompareItem(g,v,pb); }
    IFACEMETHODIMP Compare(IMFAttributes* p, MF_ATTRIBUTES_MATCH_TYPE t, BOOL* pb) override { return attrs_->Compare(p,t,pb); }
    IFACEMETHODIMP GetUINT32(REFGUID g, UINT32* pv) override                  { return attrs_->GetUINT32(g,pv); }
    IFACEMETHODIMP GetUINT64(REFGUID g, UINT64* pv) override                  { return attrs_->GetUINT64(g,pv); }
    IFACEMETHODIMP GetDouble(REFGUID g, double* pv) override                  { return attrs_->GetDouble(g,pv); }
    IFACEMETHODIMP GetGUID(REFGUID g, GUID* pv) override                      { return attrs_->GetGUID(g,pv); }
    IFACEMETHODIMP GetStringLength(REFGUID g, UINT32* pv) override            { return attrs_->GetStringLength(g,pv); }
    IFACEMETHODIMP GetString(REFGUID g, LPWSTR s, UINT32 c, UINT32* l) override { return attrs_->GetString(g,s,c,l); }
    IFACEMETHODIMP GetAllocatedString(REFGUID g, LPWSTR* s, UINT32* l) override { return attrs_->GetAllocatedString(g,s,l); }
    IFACEMETHODIMP GetBlobSize(REFGUID g, UINT32* pv) override                { return attrs_->GetBlobSize(g,pv); }
    IFACEMETHODIMP GetBlob(REFGUID g, UINT8* b, UINT32 c, UINT32* l) override { return attrs_->GetBlob(g,b,c,l); }
    IFACEMETHODIMP GetAllocatedBlob(REFGUID g, UINT8** b, UINT32* s) override { return attrs_->GetAllocatedBlob(g,b,s); }
    IFACEMETHODIMP GetUnknown(REFGUID g, REFIID r, LPVOID* pv) override       { return attrs_->GetUnknown(g,r,pv); }
    IFACEMETHODIMP SetItem(REFGUID g, REFPROPVARIANT v) override              { return attrs_->SetItem(g,v); }
    IFACEMETHODIMP DeleteItem(REFGUID g) override                             { return attrs_->DeleteItem(g); }
    IFACEMETHODIMP DeleteAllItems() override                                  { return attrs_->DeleteAllItems(); }
    IFACEMETHODIMP SetUINT32(REFGUID g, UINT32 v) override                   { return attrs_->SetUINT32(g,v); }
    IFACEMETHODIMP SetUINT64(REFGUID g, UINT64 v) override                   { return attrs_->SetUINT64(g,v); }
    IFACEMETHODIMP SetDouble(REFGUID g, double v) override                   { return attrs_->SetDouble(g,v); }
    IFACEMETHODIMP SetGUID(REFGUID g, REFGUID v) override                    { return attrs_->SetGUID(g,v); }
    IFACEMETHODIMP SetString(REFGUID g, LPCWSTR v) override                  { return attrs_->SetString(g,v); }
    IFACEMETHODIMP SetBlob(REFGUID g, const UINT8* b, UINT32 c) override     { return attrs_->SetBlob(g,b,c); }
    IFACEMETHODIMP SetUnknown(REFGUID g, IUnknown* p) override               { return attrs_->SetUnknown(g,p); }
    IFACEMETHODIMP LockStore() override                                      { return attrs_->LockStore(); }
    IFACEMETHODIMP UnlockStore() override                                    { return attrs_->UnlockStore(); }
    IFACEMETHODIMP GetCount(UINT32* p) override                              { return attrs_->GetCount(p); }
    IFACEMETHODIMP GetItemByIndex(UINT32 i, GUID* g, PROPVARIANT* pv) override { return attrs_->GetItemByIndex(i,g,pv); }
    IFACEMETHODIMP CopyAllItems(IMFAttributes* p) override                   { return attrs_->CopyAllItems(p); }

    // IMFActivate
    IFACEMETHODIMP ActivateObject(REFIID riid, void** ppv) override;
    IFACEMETHODIMP ShutdownObject() override;
    IFACEMETHODIMP DetachObject() override;

private:
    MtgVCamActivate() = default;
    ~MtgVCamActivate() { if (source_) source_->Release(); }

    volatile LONG         ref_     = 1;
    ComPtr<IMFAttributes> attrs_;
    MtgVCamSource*        source_  = nullptr;
};

#endif // _WIN32
