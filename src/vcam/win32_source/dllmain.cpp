#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <ks.h>
#include <ksmedia.h>
#include "guids.hpp"
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
    // Get the path of this DLL
    wchar_t dllPath[MAX_PATH] = {};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    // Format CLSID as string
    wchar_t clsidStr[64] = {};
    StringFromGUID2(CLSID_MtgVCamSource, clsidStr, 64);

    // Register under HKCU (no admin needed, works for CurrentUser virtual cameras)
    std::wstring base = std::wstring(L"Software\\Classes\\CLSID\\") + clsidStr;
    HRESULT hr = RegSetSZ(HKEY_CURRENT_USER, base.c_str(), nullptr, L"MTG Sim Virtual Camera");
    if (FAILED(hr)) return hr;

    std::wstring inproc = base + L"\\InprocServer32";
    hr = RegSetSZ(HKEY_CURRENT_USER, inproc.c_str(), nullptr, dllPath);
    if (FAILED(hr)) return hr;
    hr = RegSetSZ(HKEY_CURRENT_USER, inproc.c_str(), L"ThreadingModel", L"Both");
    if (FAILED(hr)) return hr;

    // Register in the camera category so Windows enumerates it
    // KSCATEGORY_VIDEO_CAMERA = {E5323777-F976-4f5b-9B55-B94699C46E44}
    // KSCATEGORY_VIDEO        = {6994AD05-93EF-11D0-A3CC-00A0C9223196}
    wchar_t videoCamStr[64] = {}, videoStr[64] = {};
    StringFromGUID2(KSCATEGORY_VIDEO_CAMERA, videoCamStr, 64);
    StringFromGUID2(KSCATEGORY_VIDEO,        videoStr,    64);

    for (const wchar_t* cat : {videoCamStr, videoStr}) {
        std::wstring catKey = std::wstring(
            L"Software\\Classes\\CLSID\\") + cat + L"\\Instance\\" + clsidStr;
        hr = RegSetSZ(HKEY_CURRENT_USER, catKey.c_str(), nullptr, L"MTG Sim Virtual Camera");
        if (FAILED(hr)) return hr;
        hr = RegSetSZ(HKEY_CURRENT_USER, catKey.c_str(), L"CLSID", clsidStr);
        if (FAILED(hr)) return hr;
        hr = RegSetSZ(HKEY_CURRENT_USER, catKey.c_str(), L"FriendlyName", L"MTG Sim");
        if (FAILED(hr)) return hr;
    }

    return S_OK;
}

STDAPI DllUnregisterServer() {
    wchar_t clsidStr[64] = {};
    StringFromGUID2(CLSID_MtgVCamSource, clsidStr, 64);

    wchar_t videoCamStr[64] = {}, videoStr[64] = {};
    StringFromGUID2(KSCATEGORY_VIDEO_CAMERA, videoCamStr, 64);
    StringFromGUID2(KSCATEGORY_VIDEO,        videoStr,    64);

    // Remove category registrations
    for (const wchar_t* cat : {videoCamStr, videoStr}) {
        std::wstring catKey = std::wstring(
            L"Software\\Classes\\CLSID\\") + cat + L"\\Instance\\" + clsidStr;
        RegDeleteTreeW(HKEY_CURRENT_USER, catKey.c_str());
    }

    // Remove CLSID registration
    std::wstring base = std::wstring(L"Software\\Classes\\CLSID\\") + clsidStr;
    RegDeleteTreeW(HKEY_CURRENT_USER, base.c_str());

    return S_OK;
}

#endif // _WIN32
