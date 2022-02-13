#include "ColorSpace.h"
#include <vulkan/vulkan.hpp>
#include <android/native_window.h>

using namespace Graphics;

ColorSpace::ColorSpace() = default;

ColorSpace::~ColorSpace() = default;

bool
ColorSpace::convert(void *buffer0, void *buffer1, void *buffer2, int32_t yStride, int32_t uvStride,
                    int32_t uvPixelStride) const {
    if (memory == nullptr) {
        return false;
    }
    switch (format) {
        case WINDOW_FORMAT_RGBA_8888:
        case WINDOW_FORMAT_RGBX_8888:
            for (int32_t y = 0; y < extent.height; ++y) {
                auto *row = reinterpret_cast<unsigned char *>((char *) memory +
                                                              layout.rowPitch *
                                                              y);
                auto *src = reinterpret_cast<unsigned char *>((char *) buffer0 +
                                                              yStride * y);
                memcpy(row, src, (size_t) (4 * extent.width));
            }
            break;
        case WINDOW_FORMAT_RGB_565:
            memcpy(memory, buffer0, size);
            break;
        case 35: { // ImageFormat.YUV_420_888
            for (int32_t y = 0; y < extent.height; ++y) {
                auto *row = reinterpret_cast<unsigned char *>((char *) memory +
                                                              layout.rowPitch *
                                                              y);
                auto *src = reinterpret_cast<unsigned char *>((char *) buffer0 +
                                                              yStride * y);
                memcpy(row, src, (size_t) (4 * extent.width));
            }
            break;
        }
        default:
            throw std::runtime_error("unsupported formats");
    }
    return true;
}

vk::Format ColorSpace::GetFormat() const {
    switch (format) {
        case WINDOW_FORMAT_RGBA_8888:
            return vk::Format::eR8G8B8A8Unorm;
        case WINDOW_FORMAT_RGBX_8888:
            return vk::Format::eR8G8B8A8Unorm;
        case WINDOW_FORMAT_RGB_565:
            return vk::Format::eR5G6B5UnormPack16;
        case 35: // ImageFormat.YUV_420_888
            return vk::Format::eG8B8R82Plane420Unorm;
        default:
            return vk::Format::eR8G8B8A8Unorm;
    }
}

int8_t ColorSpace::GetPlaneCount() const {
    switch (format) {
        case 35:
            return 3;
        default:
            return 1;
    }
}
