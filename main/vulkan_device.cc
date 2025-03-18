#include "vulkan_device.h"

#include <algorithm>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <set>
#include <string>


VkSurfaceFormatKHR SwapChainSupportDetails::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    for (const auto& format : available_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return available_formats[0];
}

VkPresentModeKHR SwapChainSupportDetails::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) {
    for (const auto& present_mode : available_present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChainSupportDetails::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }
}

SwapChainSupportDetails SwapChainSupportDetails::querySupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;
    
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
    }

    return details;
}

VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface)
    : surface_(surface) {
    pickPhysicalDevice(instance);
    queue_family_indices_ = QueueFamilyIndices(physical_device_, surface);
    createLogicalDevice();
    // command_pool_ = createCommandPool();
}

VulkanDevice::~VulkanDevice() {
    // vkDestroyCommandPool(logical_device_, command_pool_, nullptr);
    vkDestroyDevice(logical_device_, nullptr);
}

void VulkanDevice::createLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    const float queue_priority = 1.0f;

    for (uint32_t queue_family : queue_family_indices_.getUniqueFamilies()) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pEnabledFeatures = &device_features;

    create_info.enabledExtensionCount = kDeviceExtensions.size();
    create_info.ppEnabledExtensionNames = kDeviceExtensions.data();

    if (kEnableValidationLayers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(std::size(kValidationLayers));
        create_info.ppEnabledLayerNames = kValidationLayers;
    } else {
        create_info.enabledLayerCount = 0;
    }

    VK_CHECK_RESULT(vkCreateDevice(physical_device_, &create_info, nullptr, &logical_device_));

    vkGetDeviceQueue(logical_device_, queue_family_indices_.graphics_family.value(), 0, &graphics_queue_);
    vkGetDeviceQueue(logical_device_, queue_family_indices_.presentation_family.value(), 0, &presentation_queue_);
}

VkFormat VulkanDevice::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

VkQueue& VulkanDevice::getGraphicsQueue() {
    return graphics_queue_;
}

VkQueue& VulkanDevice::getPresentationQueue() {
    return presentation_queue_;
}

VkPhysicalDevice VulkanDevice::getPhysicalDevice() {
    return physical_device_;
}

uint32_t VulkanDevice::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        bool check_type = type_filter & (1 << i);
        bool check_properties = (mem_properties.memoryTypes[i].propertyFlags & properties) == properties;
        if (check_type && check_properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}    

VkCommandBuffer VulkanDevice::beginCommandBuffer() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool_;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    VK_CHECK_RESULT(vkAllocateCommandBuffers(logical_device_, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK_RESULT(vkBeginCommandBuffer(command_buffer, &begin_info));

    return command_buffer;
}

void VulkanDevice::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory) {
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = 0;
    
    VK_CHECK_RESULT(vkCreateImage(logical_device_, &image_info, nullptr, &image));

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(logical_device_, image, &mem_requirements);
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits, properties);

    VK_CHECK_RESULT(vkAllocateMemory(logical_device_, &alloc_info, nullptr, &image_memory));

    vkBindImageMemory(logical_device_, image, image_memory, 0);
}

void VulkanDevice::submitCommandBuffer(VkCommandBuffer command_buffer, VkQueue queue) {
    if (command_buffer == VK_NULL_HANDLE) {
        return;
    }

    VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0;
    VkFence fence;
    VK_CHECK_RESULT(vkCreateFence(logical_device_, &fence_info, nullptr, &fence));
    VK_CHECK_RESULT(vkQueueSubmit(graphics_queue_, 1, &submit_info, fence));
    VK_CHECK_RESULT(vkWaitForFences(logical_device_, 1, &fence, VK_TRUE, UINT64_MAX));
    vkDestroyFence(logical_device_, fence, nullptr);
    vkFreeCommandBuffers(logical_device_, command_pool_, 1, &command_buffer);
}

VkCommandPool VulkanDevice::createCommandPool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family_indices_.graphics_family.value();

    VkCommandPool command_pool;
    vkCreateCommandPool(logical_device_, &pool_info, nullptr, &command_pool);
    
    return command_pool;
}

void VulkanDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(logical_device_, &buffer_info, nullptr, &buffer));

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(logical_device_, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits, properties);

    VK_CHECK_RESULT(vkAllocateMemory(logical_device_, &alloc_info, nullptr, &buffer_memory));

    vkBindBufferMemory(logical_device_, buffer, buffer_memory, 0);
}

void VulkanDevice::createBuffer(Buffer& buffer) {
    return createBuffer(buffer.size, buffer.usage_flags, buffer.property_flags, buffer.buffer, buffer.memory);
}

VkPhysicalDevice VulkanDevice::pickPhysicalDevice(VkInstance instance) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    if (device_count == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physical_device_ = device;
            return device;
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices(device, surface_);
    if (!indices.isComplete())
        return false;

    bool extensions_supported = checkDeviceExtensionSupport(device);
    if (!extensions_supported)
        return false;

    bool swap_chain_adequate = false;
    SwapChainSupportDetails swap_chain_support = SwapChainSupportDetails::querySupport(device, surface_);
    swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
    if (!swap_chain_adequate)
        return false;

    VkPhysicalDeviceFeatures supported_features;
    vkGetPhysicalDeviceFeatures(device, &supported_features);
    if (!supported_features.samplerAnisotropy)
        return false;

    return true;
}

bool VulkanDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(kDeviceExtensions.begin(), kDeviceExtensions.end());

    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}