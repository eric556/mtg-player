#pragma once
// Shared memory layout used by both the main app (writer) and the COM DLL (reader).
// Lives in a named file-mapping "Global\MtgSimVCam".
// A named mutex "Global\MtgSimVCamMutex" guards concurrent access.
// Global\ namespace is required so the Frame Server service (Session 0) can access
// objects created by the main app (user session).
//
// Pixel format: top-down BGRA, 4 bytes per pixel.
// MFVideoFormat_RGB32 is used (not ARGB32) so the A=0 bytes from OpenGL are ignored.

#include <cstddef>
#include <cstdint>

static constexpr unsigned VCAM_SHM_MAX_W = 1920;
static constexpr unsigned VCAM_SHM_MAX_H = 1080;

static constexpr wchar_t VCAM_SHM_NAME[]   = L"Global\\MtgSimVCam";
static constexpr wchar_t VCAM_MUTEX_NAME[] = L"Global\\MtgSimVCamMutex";

#pragma pack(push, 1)
struct VcamSharedMem {
    volatile uint32_t frame_count; // incremented by app after each complete write
    uint32_t          width;
    uint32_t          height;
    uint8_t           bgra[VCAM_SHM_MAX_W * VCAM_SHM_MAX_H * 4]; // top-down BGRA
};
#pragma pack(pop)

static constexpr std::size_t VCAM_SHM_SIZE = sizeof(VcamSharedMem);
