#ifdef _WIN32
#include "vcam_win32.hpp"
#include "vcam_shared_mem.hpp"
#include "win32_source/guids.hpp"
DEFINE_CLSID_MtgVCamSource()   // one definition per binary (exe side)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <ks.h>
#include <ksmedia.h>
#include <mfvirtualcamera.h>
#include <mfapi.h>
#include <shlwapi.h>   // PathRemoveFileSpecW
#include <iostream>
#include <string>
#include <cstring>

#pragma comment(lib, "shlwapi.lib")

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::wstring exeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    return buf;
}

static std::wstring dllPath() {
    return exeDir() + L"\\mtg-sim-vcam.dll";
}

// Run DllRegisterServer from our DLL in-process (no regsvr32, no UAC needed
// for HKCU registration used by MFVirtualCameraAccess_CurrentUser).
static bool registerDll(const std::wstring& path) {
    HMODULE hMod = LoadLibraryW(path.c_str());
    if (!hMod) {
        std::cerr << "vcam-win32: cannot load " << std::string(path.begin(), path.end()) << "\n";
        return false;
    }
    using Fn = HRESULT(STDAPICALLTYPE*)();
    auto fn = reinterpret_cast<Fn>(GetProcAddress(hMod, "DllRegisterServer"));
    bool ok = fn && SUCCEEDED(fn());
    FreeLibrary(hMod);
    if (!ok) std::cerr << "vcam-win32: DllRegisterServer failed\n";
    return ok;
}

// ---------------------------------------------------------------------------
// VcamWin32
// ---------------------------------------------------------------------------

VcamWin32::VcamWin32() = default;

VcamWin32::~VcamWin32() { stop(); }

bool VcamWin32::start(unsigned width, unsigned height, int /*fps*/) {
    stop();
    width_  = width;
    height_ = height;

    // ── 1. Open (or create) the named mutex ──────────────────────────────
    mutex_handle_ = CreateMutexW(nullptr, FALSE, VCAM_MUTEX_NAME);
    if (!mutex_handle_) {
        std::cerr << "vcam-win32: CreateMutex failed (" << GetLastError() << ")\n";
        return false;
    }

    // ── 2. Create the shared memory mapping ──────────────────────────────
    shm_handle_ = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE,
        0, static_cast<DWORD>(VCAM_SHM_SIZE),
        VCAM_SHM_NAME);
    if (!shm_handle_) {
        std::cerr << "vcam-win32: CreateFileMapping failed (" << GetLastError() << ")\n";
        return false;
    }
    shm_view_ = MapViewOfFile(shm_handle_, FILE_MAP_WRITE, 0, 0, VCAM_SHM_SIZE);
    if (!shm_view_) {
        std::cerr << "vcam-win32: MapViewOfFile failed (" << GetLastError() << ")\n";
        return false;
    }

    // Write initial dimensions so the DLL knows resolution before first frame
    auto* shm = static_cast<VcamSharedMem*>(shm_view_);
    shm->frame_count = 0;
    shm->width       = width;
    shm->height      = height;

    // ── 3. Register DLL + create virtual camera ───────────────────────────
    if (!ensureDllRegistered()) return false;
    if (!createVirtualCamera()) return false;

    running_ = true;
    std::cout << "vcam-win32: virtual camera started ("
              << width << "x" << height << ") — look for \"MTG Sim\" in camera settings\n";
    return true;
}

void VcamWin32::stop() {
    destroyVirtualCamera();
    if (shm_view_)    { UnmapViewOfFile(shm_view_);  shm_view_    = nullptr; }
    if (shm_handle_)  { CloseHandle(shm_handle_);    shm_handle_  = nullptr; }
    if (mutex_handle_){ CloseHandle(mutex_handle_);  mutex_handle_= nullptr; }
    running_ = false;
}

void VcamWin32::pushFrame(const uint8_t* bgra, unsigned width, unsigned height) {
    if (!shm_view_ || !mutex_handle_) return;

    DWORD wait = WaitForSingleObject(mutex_handle_, 16); // 16ms timeout
    if (wait != WAIT_OBJECT_0) return;

    auto* shm = static_cast<VcamSharedMem*>(shm_view_);
    shm->width  = width;
    shm->height = height;
    const std::size_t bytes = static_cast<std::size_t>(width) * height * 4;
    const std::size_t maxBytes = sizeof(shm->bgra);
    std::memcpy(shm->bgra, bgra, bytes < maxBytes ? bytes : maxBytes);
    shm->frame_count++;

    ReleaseMutex(mutex_handle_);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool VcamWin32::ensureDllRegistered() {
    // Check if our CLSID is already registered under HKCU.
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_MtgVCamSource, clsidStr, 64);
    std::wstring keyPath = std::wstring(L"Software\\Classes\\CLSID\\") + clsidStr;
    HKEY hk = nullptr;
    bool already = (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS);
    if (hk) RegCloseKey(hk);
    if (already) return true;

    return registerDll(dllPath());
}

bool VcamWin32::createVirtualCamera() {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::cerr << "vcam-win32: MFStartup failed (0x" << std::hex << hr << ")\n";
        return false;
    }

    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_MtgVCamSource, clsidStr, 64);

    // Camera categories: Video capture + virtual camera
    GUID cats[] = { KSCATEGORY_VIDEO_CAMERA };

    IMFVirtualCamera* vcam = nullptr;
    hr = MFCreateVirtualCamera(
        MFVirtualCameraType_SoftwareCameraSource,
        MFVirtualCameraLifetime_Session,
        MFVirtualCameraAccess_CurrentUser,
        L"MTG Sim",
        clsidStr,
        cats,
        ARRAYSIZE(cats),
        &vcam);

    if (FAILED(hr)) {
        std::cerr << "vcam-win32: MFCreateVirtualCamera failed (0x"
                  << std::hex << hr << ")\n"
                  << "  Make sure mtg-sim-vcam.dll is next to the exe and is registered.\n";
        MFShutdown();
        return false;
    }

    hr = vcam->Start(nullptr);
    if (FAILED(hr)) {
        std::cerr << "vcam-win32: IMFVirtualCamera::Start failed (0x" << std::hex << hr << ")\n";
        vcam->Release();
        MFShutdown();
        return false;
    }

    vcam_handle_ = vcam;
    return true;
}

void VcamWin32::destroyVirtualCamera() {
    if (!vcam_handle_) return;
    auto* vcam = static_cast<IMFVirtualCamera*>(vcam_handle_);
    vcam->Stop();
    vcam->Remove();
    vcam->Release();
    vcam_handle_ = nullptr;
    MFShutdown();
}

#endif // _WIN32
