#pragma once
#ifdef _WIN32
#include "vcam.hpp"
#include <cstdint>
#include <string>

// Windows-only Vcam implementation using IMFVirtualCamera + named shared memory.
// The COM media-source DLL (mtg-sim-vcam.dll) is registered on first use and
// loaded by the Windows Frame Server when any client (e.g. Chrome) opens the
// virtual camera device.
class VcamWin32 : public Vcam {
public:
    VcamWin32();
    ~VcamWin32() override;

    bool start(unsigned width, unsigned height, int fps) override;
    void stop() override;
    bool isRunning() const override { return running_; }
    void pushFrame(const uint8_t* bgra, unsigned width, unsigned height) override;

private:
    bool ensureDllRegistered();   // registers COM DLL if not already done
    bool createVirtualCamera();   // calls MFCreateVirtualCamera
    void destroyVirtualCamera();

    void*    shm_handle_   = nullptr; // HANDLE to the file mapping
    void*    shm_view_     = nullptr; // mapped view pointer (VcamSharedMem*)
    void*    mutex_handle_ = nullptr; // named mutex for SHM access
    void*    vcam_handle_  = nullptr; // IMFVirtualCamera*
    bool     running_      = false;
    unsigned width_        = 0;
    unsigned height_       = 0;
};
#endif // _WIN32
