#include "Kernel.h"
#include "SwapChain.h"
#include "Texture.h"
#include "stdexcept"
#include <picojson/picojson.h>
#include <jni.h>
#include "../haishinkit.hpp"
#include "vulkan/vulkan.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_android.h"
#include "DynamicLoader.h"
#include <android/native_window.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace Graphics;

Kernel::Kernel() : isValidationLayersEnabled(true), assetManager(nullptr),
                   featureManager(new FeatureManager()) {
    if (!DynamicLoader::GetInstance().Load()) {
        return;
    }

    const auto useValidationLayers = isValidationLayersEnabled && IsValidationLayersSupported();
    if (useValidationLayers) {
        featureManager->features.emplace_back(new DebugUtilsMessengerFeature());
    }

    const auto appInfo = vk::ApplicationInfo()
            .setPNext(nullptr)
            .setPApplicationName(applicationName.c_str())
            .setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
            .setPEngineName(engineName.c_str())
            .setEngineVersion(VK_MAKE_VERSION(1, 0, 0))
            .setApiVersion(VK_API_VERSION_1_2);

    std::vector<const char *> extensions = featureManager->GetExtensions(INSTANCE);

    auto createInfo = vk::InstanceCreateInfo()
            .setFlags(vk::InstanceCreateFlags())
            .setPApplicationInfo(&appInfo)
            .setEnabledLayerCount(0)
            .setPEnabledLayerNames(nullptr)
            .setEnabledExtensionCount(extensions.size())
            .setPEnabledExtensionNames(extensions)
            .setPNext(featureManager->GetNext(INSTANCE));

    if (useValidationLayers) {
        createInfo
                .setEnabledLayerCount(validationLayers.size())
                .setPpEnabledLayerNames(validationLayers.data());
    }

    instance = vk::createInstanceUnique(createInfo);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());
    isAvailable = true;
}

Kernel::~Kernel() {
    if (isAvailable) {
        device->waitIdle();
    }
    TearDown();
}

void Kernel::SetAssetManager(AAssetManager *newAssetManager) {
    assetManager = newAssetManager;
}

void Kernel::SetTextures(const std::vector<Texture *> &textures) {
    if (!(isAvailable && 0 <= selectedPhysicalDevice)) {
        return;
    }
    for (auto *texture: textures) {
        texture->SetUp(*this);
    }
    pipeline.SetTextures(*this, textures);
    commandBuffer.SetTextures(*this, textures);
}

void Kernel::SetUp(ANativeWindow *nativeWindow) {
    if (!isAvailable) {
        return;
    }
    SelectPhysicalDevice();

    surface = instance->createAndroidSurfaceKHRUnique(
            vk::AndroidSurfaceCreateInfoKHR()
                    .setFlags(vk::AndroidSurfaceCreateFlagsKHR())
                    .setPNext(nullptr)
                    .setWindow(nativeWindow));

    swapChain.SetUp(*this);
    pipeline.SetUp(*this);
    queue.SetImagesCount(*this, swapChain.GetImagesCount());
    commandBuffer.SetUp(*this);
}

void Kernel::TearDown() {
    if (!isAvailable) {
        return;
    }
    device->waitIdle();
    commandBuffer.TearDown(*this);
    pipeline.TearDown(*this);
    swapChain.TearDown(*this);
}

vk::Result Kernel::DrawFrame() {
    if (!isAvailable) {
        return vk::Result::eErrorInitializationFailed;
    }
    uint32_t index = queue.Acquire(*this);
    return queue.Present(*this, index, commandBuffer.commandBuffers[index].get());
}

bool Kernel::IsAvailable() const {
    return isAvailable;
}

SurfaceRotation Kernel::GetSurfaceRotation() {
    return surfaceRotation;
}

void Kernel::SetSurfaceRotation(SurfaceRotation newSurfaceRotation) {
    surfaceRotation = newSurfaceRotation;
    invalidateSurfaceRotation = true;
}

vk::ShaderModule Kernel::LoadShader(const std::string &fileName) {
    const auto code = ReadFile(fileName);
    return device->createShaderModule(
            vk::ShaderModuleCreateInfo()
                    .setCodeSize(code.size())
                    .setPCode(reinterpret_cast<const uint32_t *>(code.data()))
    );
}

bool Kernel::IsValidationLayersSupported() {
    const auto properties = vk::enumerateInstanceLayerProperties();
    for (auto layerName : validationLayers) {
        for (auto layerProperties : properties) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                return true;
            }
        }
    }
    return false;
}

void Kernel::SelectPhysicalDevice() {
    if (0 <= selectedPhysicalDevice) {
        return;
    }

    auto physicalDevices = instance->enumeratePhysicalDevices();
    for (auto i = 0; i < physicalDevices.size(); i++) {
        this->physicalDevice = physicalDevices[i];
        selectedPhysicalDevice = i;
        break;
    }
    uint32_t propertyCount;
    physicalDevice.getQueueFamilyProperties(&propertyCount, nullptr);
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    size_t graphicsQueueFamilyIndex = std::distance(
            queueFamilyProperties.begin(),
            std::find_if(
                    queueFamilyProperties.begin(), queueFamilyProperties.end(),
                    [](vk::QueueFamilyProperties const &qfp) {
                        return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                    }));
    assert(graphicsQueueFamilyIndex < queueFamilyProperties.size());

    std::vector<float> queuePriorities{1.0f};
    std::vector<const char *> extensions = featureManager->GetExtensions(DEVICE);

    device = physicalDevice.createDeviceUnique(
            vk::DeviceCreateInfo()
                    .setPNext(featureManager->GetNext(FEATURE))
                    .setEnabledExtensionCount(extensions.size())
                    .setPpEnabledExtensionNames(extensions.data())
                    .setQueueCreateInfoCount(1)
                    .setQueueCreateInfos(
                            vk::DeviceQueueCreateInfo()
                                    .setFlags(vk::DeviceQueueCreateFlags())
                                    .setQueueFamilyIndex(
                                            static_cast<uint32_t>( graphicsQueueFamilyIndex ))
                                    .setQueueCount(1)
                                    .setQueuePriorities(queuePriorities)
                    )
    );
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());
    queue.SetUp(*this, graphicsQueueFamilyIndex);
}

void Kernel::Submit(vk::CommandBuffer &commandBuffer) {
    queue.Submit(*this, commandBuffer);
}

vk::UniqueImageView Kernel::CreateImageView(vk::Image image, vk::Format format) {
    return device->createImageViewUnique(
            vk::ImageViewCreateInfo()
                    .setImage(image)
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(format)
                    .setComponents(
                            vk::ComponentMapping()
                                    .setR(vk::ComponentSwizzle::eR)
                                    .setG(vk::ComponentSwizzle::eG)
                                    .setB(vk::ComponentSwizzle::eB)
                                    .setA(vk::ComponentSwizzle::eA))
                    .setSubresourceRange(
                            vk::ImageSubresourceRange()
                                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                    .setBaseMipLevel(0)
                                    .setLevelCount(1)
                                    .setBaseArrayLayer(0)
                                    .setLayerCount(1)));
}

uint32_t
Kernel::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
    auto memoryProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void Kernel::ReadPixels(void *buffer) {
}

std::string Kernel::InspectDevices() {
    picojson::object inspect;
    picojson::array devices;
    for (const auto &device: instance->enumeratePhysicalDevices()) {
        picojson::object data;
        const auto properties = device.getProperties();
        data.emplace(std::make_pair("device_name", (std::string) properties.deviceName));
        if (properties.deviceType == vk::PhysicalDeviceType::eCpu) {
            data.emplace(std::make_pair("device_type", "cpu"));
        } else if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
            data.emplace(std::make_pair("device_type", "integrated_gpu"));
        } else if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            data.emplace(std::make_pair("device_type", "discrete_gpu"));
        } else if (properties.deviceType == vk::PhysicalDeviceType::eVirtualGpu) {
            data.emplace(std::make_pair("device_type", "virtual_gpu"));
        } else {
            data.emplace(std::make_pair("device_type", "other"));
        }
        data.emplace(std::make_pair("api_version", (double) properties.apiVersion));
        data.emplace(std::make_pair("driver_version", (double) properties.driverVersion));
        data.emplace(std::make_pair("vendor_id", (double) properties.vendorID));
        data.emplace(std::make_pair("device_id", (double) properties.deviceID));
        picojson::array extensions;
        for (const auto extension : device.enumerateDeviceExtensionProperties()) {
            extensions.emplace_back((std::string) extension.extensionName);
        }
        data.emplace(std::make_pair("extensions", extensions));
        devices.push_back(picojson::value(data));
    }
    inspect.emplace(std::make_pair("devices", devices));
    return picojson::value(inspect).serialize();
}

std::vector<char> Kernel::ReadFile(const std::string &fileName) {
    if (assetManager == nullptr) {
        throw std::runtime_error("");
    }
    AAsset *file = AAssetManager_open(assetManager, fileName.c_str(), AASSET_MODE_BUFFER);
    if (file == nullptr) {
        throw std::runtime_error("");
    }
    const auto length = AAsset_getLength(file);
    std::vector<char> contents(length);
    AAsset_read(file, static_cast<void *>(contents.data()), length);
    AAsset_close(file);
    return contents;
}

