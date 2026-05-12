#pragma once
#include <vcam.h>
#include <vector>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <iostream>

class VCamWrapper {
public:
    VCamWrapper() : vcam_(nullptr), width_(0), height_(0) {}
    ~VCamWrapper() {
        stop();
    }

    bool start(unsigned int width, unsigned int height) {
        if (vcam_ && width_ == width && height_ == height) return true;
        
        stop();
        
        width_ = width;
        height_ = height;
        
        vcam_ = vcam_open();
        if (!vcam_) {
            std::cerr << "[VCam] Failed to open VCam driver.\n";
            return false;
        }
        
        if (vcam_start(vcam_, width_, height_) != 0) {
            std::cerr << "[VCam] Failed to start VCam stream.\n";
            stop();
            return false;
        }
        
        nv12_buffer_.resize(width_ * height_ * 3 / 2);
        std::cout << "[VCam] Virtual Camera started at " << width_ << "x" << height_ << "\n";
        return true;
    }

    void stop() {
        if (vcam_) {
            vcam_stop(vcam_);
            free(vcam_);
        }
        vcam_ = nullptr;
    }

    void pushFrame(const sf::Image& image) {
        if (!vcam_) return;
        
        auto size = image.getSize();
        if (size.x != width_ || size.y != height_) return;

        rgbaToNv12(image);
        vcam_write_frame(vcam_, nv12_buffer_.data());
    }

    bool isRunning() const { return vcam_ != nullptr; }

private:
    void rgbaToNv12(const sf::Image& image) {
        const uint8_t* rgba = image.getPixelsPtr();
        uint8_t* y_plane = nv12_buffer_.data();
        uint8_t* uv_plane = y_plane + width_ * height_;

        for (unsigned int y = 0; y < height_; ++y) {
            for (unsigned int x = 0; x < width_; ++x) {
                const uint8_t* p = &rgba[(y * width_ + x) * 4];
                uint8_t r = p[0];
                uint8_t g = p[1];
                uint8_t b = p[2];

                // Y plane: standard Rec.601 YUV conversion
                y_plane[y * width_ + x] = static_cast<uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);

                // UV plane: interleaved (U, V, U, V, ...) - one pair per 2x2 block
                if (y % 2 == 0 && x % 2 == 0) {
                    uint8_t u = static_cast<uint8_t>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
                    uint8_t v = static_cast<uint8_t>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
                    
                    unsigned int uv_index = (y / 2) * width_ + x;
                    uv_plane[uv_index] = u;
                    uv_plane[uv_index + 1] = v;
                }
            }
        }
    }

    VCam* vcam_;
    unsigned int width_, height_;
    std::vector<uint8_t> nv12_buffer_;
};
