#pragma once
#include "vcam.hpp"
#include <string>
#include <cstdio>

// Vcam implementation that pipes raw BGRA frames to an ffmpeg subprocess.
// Works on all platforms; on Linux use output="/dev/video0" with v4l2loopback.
class VcamFfmpeg : public Vcam {
public:
    explicit VcamFfmpeg(std::string output) : output_(std::move(output)) {}
    ~VcamFfmpeg() override { stop(); }

    bool start(unsigned width, unsigned height, int fps) override;
    void stop() override;
    bool isRunning() const override { return pipe_ != nullptr; }
    void pushFrame(const uint8_t* bgra, unsigned width, unsigned height) override;

private:
    std::string  output_;
    FILE*        pipe_   = nullptr;
    unsigned     width_  = 0;
    unsigned     height_ = 0;
};
