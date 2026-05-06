#pragma once
// Shared memory layout used by both the main app (writer) and the COM DLL (reader).
// Lives in a named file-mapping "Local\MtgSimVCam".
// A named mutex "Local\MtgSimVCamMutex" guards concurrent access.
//
// Pixel format: top-down BGRA, 4 bytes per pixel.
// This matches MFVideoFormat_ARGB32 (positive stride) and ffmpeg's "bgra" pixel_format.

#include <cstddef>
#include <cstdint>

static constexpr unsigned VCAM_SHM_MAX_W = 1920;
static constexpr unsigned VCAM_SHM_MAX_H = 1080;

static constexpr wchar_t VCAM_SHM_NAME[]   = L"Local\\MtgSimVCam";
static constexpr wchar_t VCAM_MUTEX_NAME[] = L"Local\\MtgSimVCamMutex";

#pragma pack(push, 1)
struct VcamSharedMem {
    volatile uint32_t frame_count; // incremented by app after each complete write
    uint32_t          width;
    uint32_t          height;
    uint8_t           bgra[VCAM_SHM_MAX_W * VCAM_SHM_MAX_H * 4]; // top-down BGRA
};
#pragma pack(pop)

static constexpr std::size_t VCAM_SHM_SIZE = sizeof(VcamSharedMem);
