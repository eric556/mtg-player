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
#include <cstdio>

#pragma comment(lib, "shlwapi.lib")

// Writes a line to C:\ProgramData\mtg-vcam-debug.log (writable by SYSTEM).
static void VcamLog(const char* msg) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\ProgramData\\mtg-vcam-debug.log", "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

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
        wchar_t guidW[64] = {};
        StringFromGUID2(riid, guidW, 64);
        char guidA[64] = {};
        WideCharToMultiByte(CP_ACP, 0, guidW, -1, guidA, 64, nullptr, nullptr);
        char buf[128];
        sprintf_s(buf, "CreateInstance riid=%s", guidA);
        VcamLog(buf);
        if (outer) return CLASS_E_NOAGGREGATION;
        if (!ppv)  return E_POINTER;

        // Frame Server asks for IMFActivate; hand it an activate wrapper.
        MtgVCamActivate* act = nullptr;
        HRESULT hr = MtgVCamActivate::Create(&act);
        sprintf_s(buf, "MtgVCamActivate::Create hr=0x%08X", (unsigned)hr);
        VcamLog(buf);
        if (SUCCEEDED(hr)) {
            hr = act->QueryInterface(riid, ppv);
            sprintf_s(buf, "Activate QI hr=0x%08X", (unsigned)hr);
            VcamLog(buf);
            act->Release();
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
    VcamLog("DllGetClassObject called");
    if (rclsid != CLSID_MtgVCamSource) {
        VcamLog("DllGetClassObject: wrong CLSID");
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    HRESULT hr = g_classFactory.QueryInterface(riid, ppv);
    char buf[64];
    sprintf_s(buf, "DllGetClassObject factory QI hr=0x%08X", (unsigned)hr);
    VcamLog(buf);
    return hr;
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
