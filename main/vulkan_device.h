#pragma once

#include "main/vulkan_constants.h"
#include "vulkan/vulkan.h"
#include "main/vulkan_buffer.h"

#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct GLFWwindow;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);

    static SwapChainSupportDetails querySupport(VkPhysicalDevice device, VkSurfaceKHR surface);
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> presentation_family;

    bool isComplete() const {
        return graphics_family.has_value() && presentation_family.has_value();
    }

    QueueFamilyIndices() = default;
    QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface) {       
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        for (int i = 0; i < queue_family_count; ++i) {
            if (isComplete())
                break;

            VkBool32 presentation_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentation_support);
            if (presentation_support) {
                presentation_family = i;
            }

            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family = i;
            }            
        }
    }

    std::set<uint32_t> getUniqueFamilies() {
        std::set<uint32_t> families;
        if (graphics_family.has_value())
            families.insert(graphics_family.value());
        if (presentation_family.has_value())
            families.insert(presentation_family.value());
        return families;
    }
};

class VulkanDevice {
public:
    VulkanDevice() = default;
    VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
    ~VulkanDevice();

    void createLogicalDevice();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory);
    void createBuffer(Buffer& buffer);
    VkQueue& getGraphicsQueue();

    VkQueue& getPresentationQueue();

    VkPhysicalDevice getPhysicalDevice();

    uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

    VkCommandBuffer beginCommandBuffer();
    VkCommandPool getCommandPool() {
        return command_pool_;
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory);

    void submitCommandBuffer(VkCommandBuffer command_buffer, VkQueue queue);

    VkDevice getLogicalDevice() const {
        return logical_device_;
    }

    operator VkDevice() const {
        return logical_device_;
    }

private:
    VkPhysicalDevice pickPhysicalDevice(VkInstance instance);
    bool isDeviceSuitable(VkPhysicalDevice device);
    VkCommandPool createCommandPool();
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    QueueFamilyIndices queue_family_indices_;
    VkCommandPool command_pool_;
    VkPhysicalDevice physical_device_;
    VkDevice logical_device_;
    VkPhysicalDeviceProperties properties_;
    VkPhysicalDeviceFeatures features_;
    VkPhysicalDeviceMemoryProperties memory_properties_;
    VkSurfaceKHR surface_;
    std::vector<VkQueueFamilyProperties> queue_family_properties_;
    std::vector<std::string> supported_extensions_;
    VkQueue graphics_queue_;
    VkQueue presentation_queue_;
};