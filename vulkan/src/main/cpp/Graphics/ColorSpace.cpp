#include "Kernel.h"
#include "ImageStorage.h"
#include "ColorSpace.h"

using namespace Graphics;

ColorSpace::ColorSpace() = default;

ColorSpace::~ColorSpace() = default;

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

void ColorSpace::Bind(Kernel &kernel, ImageStorage &storage, vk::MemoryPropertyFlags properties) {
    const auto hardwareBufferProperties = kernel.device->getAndroidHardwareBufferPropertiesANDROID(
            *storage.buffer);
    storage.memory = kernel.device->allocateMemoryUnique(
            vk::MemoryAllocateInfo()
                    .setAllocationSize(hardwareBufferProperties.allocationSize)
                    .setMemoryTypeIndex(
                            kernel.FindMemoryType(
                                    hardwareBufferProperties.memoryTypeBits,
                                    properties
                            ))
                    .setPNext(&vk::MemoryDedicatedAllocateInfo()
                            .setImage(storage.image.get())
                            .setPNext(&vk::ImportAndroidHardwareBufferInfoANDROID()
                                    .setBuffer(storage.buffer)))
    );
    kernel.device->bindImageMemory(storage.image.get(), storage.memory.get(), 0);
}
