#ifndef HAISHINKIT_KT_TEXTURE_H
#define HAISHINKIT_KT_TEXTURE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vulkan/vulkan.hpp>
#include <android/native_window.h>
#include "ImageStorage.h"
#include "VideoGravity.h"
#include "ResampleFilter.h"
#include "ColorSpace.h"
#include "ImageOrientation.h"
#include "PushConstants.hpp"

namespace Graphics {
    class Kernel;

    struct ImageStorage;

    struct Texture {
        enum Mode {
            Linear,
            Stage
        };

        bool invalidateLayout = true;
        VideoGravity videoGravity = RESIZE_ASPECT;
        ResampleFilter resampleFilter = CUBIC;

        Texture(vk::Extent2D extent, int32_t format);

        ~Texture();

        void SetUp(Kernel &kernel);

        void SetImageOrientation(ImageOrientation newImageOrientation);

        void TearDown(Kernel &kernel);

        vk::Viewport GetViewport(Kernel &kernel) const;

        PushConstants GetPushConstants(Kernel &kernel) const;

        vk::DescriptorImageInfo CreateDescriptorImageInfo();

        void Update(Kernel &kernel, AHardwareBuffer *buffer);

    private:
        Mode mode = Mode::Linear;
        ImageStorage image;
        ImageStorage stage;
        vk::UniqueSampler sampler;
        vk::UniqueImageView imageView;
        ImageOrientation imageOrientation = UP;
        ColorSpace *colorSpace;

        bool HasLinearTilingFeatures(Kernel &kernel) const;

        void CopyImage(Kernel &kernel);
    };
}

#endif //HAISHINKIT_KT_TEXTURE_H
