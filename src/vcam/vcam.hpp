#pragma once
#include <cstdint>

// Abstract virtual-camera interface.
// Implementations: VcamFfmpeg (all platforms), VcamWin32 (Windows IMFVirtualCamera).
// Pixel convention: top-down BGRA, 4 bytes per pixel.
class Vcam {
public:
    virtual ~Vcam() = default;

    // Open the camera/pipe for the given frame dimensions and fps.
    // Returns false and prints an error if setup fails.
    virtual bool start(unsigned width, unsigned height, int fps) = 0;

    // Close the camera/pipe.
    virtual void stop() = 0;

    virtual bool isRunning() const = 0;

    // Push one frame. rgba must point to width*height*4 bytes of top-down BGRA.
    // Called at most fps times per second by the owner.
    virtual void pushFrame(const uint8_t* bgra, unsigned width, unsigned height) = 0;
};
