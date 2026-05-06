#include "vcam_ffmpeg.hpp"
#include <iostream>
#include <string>

bool VcamFfmpeg::start(unsigned width, unsigned height, int fps) {
    stop();
    width_  = width;
    height_ = height;

    // bgra pixel_format: top-down, 4 bytes/pixel, B G R A byte order.
    std::string cmd =
        "ffmpeg -y -f rawvideo -pixel_format bgra"
        " -video_size " + std::to_string(width) + "x" + std::to_string(height) +
        " -framerate " + std::to_string(fps) +
        " -i pipe:0 \"" + output_ + "\"";

#ifdef _WIN32
    pipe_ = _popen(cmd.c_str(), "wb");
#else
    pipe_ = popen(cmd.c_str(), "w");
#endif

    if (!pipe_) {
        std::cerr << "vcam-ffmpeg: failed to start ffmpeg — is it on PATH?\n"
                  << "  cmd: " << cmd << "\n";
        return false;
    }
    std::cout << "vcam-ffmpeg: streaming to \"" << output_ << "\" at "
              << fps << " fps (" << width << "x" << height << ")\n";
    return true;
}

void VcamFfmpeg::stop() {
    if (!pipe_) return;
#ifdef _WIN32
    _pclose(pipe_);
#else
    pclose(pipe_);
#endif
    pipe_ = nullptr;
}

void VcamFfmpeg::pushFrame(const uint8_t* bgra, unsigned width, unsigned height) {
    if (!pipe_) return;
    fwrite(bgra, 1, static_cast<std::size_t>(width) * height * 4, pipe_);
}
