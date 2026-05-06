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
#include <shellapi.h>  // ShellExecuteExW
#include <iostream>
#include <string>
#include <cstring>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

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

// Spawn regsvr32 elevated via UAC to write the InprocServer32 key into HKLM.
// Blocks until regsvr32 exits (up to 15 s).
static bool registerDllElevated(const std::wstring& dll) {
    wchar_t sysDir[MAX_PATH] = {};
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring regsvr32 = std::wstring(sysDir) + L"\\regsvr32.exe";
    std::wstring params   = L"/s \"" + dll + L"\"";

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas";
    sei.lpFile       = regsvr32.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED)
            std::cerr << "vcam-win32: UAC prompt was cancelled — re-run to register.\n";
        else
            std::cerr << "vcam-win32: ShellExecuteEx failed (" << err << ")\n";
        return false;
    }
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 15000);
        CloseHandle(sei.hProcess);
    }
    return true;
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
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_MtgVCamSource, clsidStr, 64);
    // AllUsers cameras must be in HKLM so the Frame Server service can CoCreateInstance them.
    std::wstring keyPath = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + clsidStr;
    HKEY hk = nullptr;
    bool already = (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS);
    if (hk) RegCloseKey(hk);
    if (already) return true;

    std::cout << "vcam-win32: registering COM source in HKLM (a UAC prompt will appear once)\n";
    if (!registerDllElevated(dllPath())) return false;

    hk = nullptr;
    bool ok = (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS);
    if (hk) RegCloseKey(hk);
    if (!ok) std::cerr << "vcam-win32: HKLM registration failed\n";
    return ok;
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
        MFVirtualCameraAccess_AllUsers,
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
