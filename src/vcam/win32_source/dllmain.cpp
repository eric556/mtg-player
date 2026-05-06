#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include "guids.hpp"
DEFINE_CLSID_MtgVCamSource()   // one definition per binary (DLL side)
#include "media_source.hpp"
#include <shlwapi.h>
#include <string>

#pragma comment(lib, "shlwapi.lib")

static HMODULE g_hModule = nullptr;

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// IClassFactory
// ---------------------------------------------------------------------------

class MtgVCamClassFactory : public IClassFactory {
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return 2; } // static lifetime
    IFACEMETHODIMP_(ULONG) Release() override { return 1; }

    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        if (!ppv)  return E_POINTER;
        MtgVCamSource* src = nullptr;
        HRESULT hr = MtgVCamSource::Create(&src);
        if (SUCCEEDED(hr)) {
            hr = src->QueryInterface(riid, ppv);
            src->Release();
        }
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL) override { return S_OK; }
};

static MtgVCamClassFactory g_classFactory;

// ---------------------------------------------------------------------------
// COM exports
// ---------------------------------------------------------------------------

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (rclsid != CLSID_MtgVCamSource) return CLASS_E_CLASSNOTAVAILABLE;
    return g_classFactory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() {
    return S_FALSE; // keep loaded while the Frame Server holds references
}

// ---------------------------------------------------------------------------
// Registration helpers
// ---------------------------------------------------------------------------

static HRESULT RegSetSZ(HKEY hRoot, const wchar_t* subKey,
                         const wchar_t* valueName, const wchar_t* data) {
    HKEY hKey = nullptr;
    LONG r = RegCreateKeyExW(hRoot, subKey, 0, nullptr, 0,
                              KEY_WRITE, nullptr, &hKey, nullptr);
    if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
    r = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(data),
                       static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(r);
}

STDAPI DllRegisterServer() {
    wchar_t dll[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, dll, MAX_PATH);

    wchar_t clsidStr[64] = {};
    StringFromGUID2(CLSID_MtgVCamSource, clsidStr, 64);

    // Register under HKLM so the Frame Server service (SYSTEM) can CoCreateInstance.
    // Called by regsvr32 running elevated (UAC); called once at first use.
    std::wstring base   = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + clsidStr;
    std::wstring inproc = base + L"\\InprocServer32";

    HRESULT hr = RegSetSZ(HKEY_LOCAL_MACHINE, base.c_str(), nullptr, L"MTG Sim Virtual Camera");
    if (FAILED(hr)) return hr;
    hr = RegSetSZ(HKEY_LOCAL_MACHINE, inproc.c_str(), nullptr, dll);
    if (FAILED(hr)) return hr;
    return RegSetSZ(HKEY_LOCAL_MACHINE, inproc.c_str(), L"ThreadingModel", L"Both");
}

STDAPI DllUnregisterServer() {
    wchar_t clsidStr[64] = {};
    StringFromGUID2(CLSID_MtgVCamSource, clsidStr, 64);
    std::wstring base = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + clsidStr;
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, base.c_str());
    return S_OK;
}

#endif // _WIN32
