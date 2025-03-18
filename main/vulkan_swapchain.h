#pragma once

#include <memory>

#include "vulkan_device.h"


struct GLFWwindow;

class VulkanSwapchain {
public:
    ~VulkanSwapchain();
    static std::unique_ptr<VulkanSwapchain> createSwapChain(
        VulkanDevice* device, VkSurfaceKHR surface, GLFWwindow* window);

    VkFormat getImageFormat();
    VkFormat getDepthFormat();
    VkExtent2D getExtent();
    uint32_t getImageCount();
    VkImageView getImageView(size_t index);
    VkImageView getDepthImageView();

    operator VkSwapchainKHR() const {
        return swap_chain_;
    }

private:
    VulkanSwapchain(VulkanDevice* device, VkSwapchainKHR swap_chain, VkExtent2D extent, VkFormat format, std::vector<VkImage>& swap_chain_images); 

    void createImageViews();

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
    VkFormat findDepthFormat();

    void createDepthResources();

    VkSwapchainKHR swap_chain_;
    VkExtent2D swap_chain_extent_;
    std::vector<VkImage> swap_chain_images_;
    std::vector<VkImageView> swap_chain_image_views_;
    VkImage depth_image_;
    VkImageView depth_image_view_;
    VkDeviceMemory depth_image_memory_;
    VkFormat depth_format_;
    VkFormat image_format_;
    VulkanDevice* device_;
};