#include "Kernel.h"
#include "Util.h"
#include "ImageStorage.h"

using namespace Graphics;

void ImageStorage::SetUp(Kernel &kernel, vk::ImageCreateInfo info) {
    layout = info.initialLayout;
    image = kernel.device->createImageUnique(info);

    AHardwareBuffer_Desc desc{
            .width = info.extent.width,
            .height = info.extent.height,
            .layers = info.arrayLayers,
            .format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
            .usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                     AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK |
                     AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER,
            .stride = 0,
    };

    AHardwareBuffer_allocate(&desc, &buffer);
}

void ImageStorage::TearDown(Kernel &kernel) {
}

void ImageStorage::SetLayout(vk::CommandBuffer &commandBuffer,
                             vk::ImageLayout newImageLayout,
                             vk::PipelineStageFlagBits srcStageMask,
                             vk::PipelineStageFlagBits dstStageMask) {

    const auto barrier = Util::CreateImageMemoryBarrier(layout, newImageLayout)
            .setImage(image.get())
            .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    commandBuffer.pipelineBarrier(
            srcStageMask,
            dstStageMask,
            vk::DependencyFlags(),
            nullptr,
            nullptr,
            barrier
    );

    layout = newImageLayout;
}

vk::ImageCreateInfo ImageStorage::CreateImageCreateInfo() const {
    return vk::ImageCreateInfo()
            .setImageType(vk::ImageType::e2D)
            .setExtent(vk::Extent3D(extent.width, extent.height, 1))
            .setMipLevels(1)
            .setArrayLayers(1)
            .setFormat(format)
            .setInitialLayout(layout)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setSamples(vk::SampleCountFlagBits::e1);
}
