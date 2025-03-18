#include "vulkan_swapchain.h"

std::unique_ptr<VulkanSwapchain> VulkanSwapchain::createSwapChain(
    VulkanDevice* device, VkSurfaceKHR surface, GLFWwindow* window) {
    SwapChainSupportDetails swap_chain_support = SwapChainSupportDetails::querySupport(device->getPhysicalDevice(), surface);

    VkSurfaceFormatKHR surface_format = swap_chain_support.chooseSwapSurfaceFormat(swap_chain_support.formats);
    VkPresentModeKHR present_mode = swap_chain_support.chooseSwapPresentMode(swap_chain_support.present_modes);
    VkExtent2D extent = swap_chain_support.chooseSwapExtent(swap_chain_support.capabilities, window);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount) {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info;
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices(device->getPhysicalDevice(), surface);
    uint32_t queue_family_indices[] = {indices.graphics_family.value(), indices.presentation_family.value()};

    if (indices.graphics_family != indices.presentation_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    create_info.pNext = nullptr;
    create_info.flags = 0;

    VkSwapchainKHR swap_chain;
    VK_CHECK_RESULT(vkCreateSwapchainKHR(*device, &create_info, nullptr, &swap_chain));
    std::vector<VkImage> swap_chain_images;
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(*device, swap_chain, &image_count, nullptr));
    swap_chain_images.resize(image_count);
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(*device, swap_chain, &image_count, swap_chain_images.data()));

    return std::unique_ptr<VulkanSwapchain>(new VulkanSwapchain(device, swap_chain, extent, surface_format.format, swap_chain_images));
}

VkFormat VulkanSwapchain::getImageFormat() {
    return image_format_;
}

VkFormat VulkanSwapchain::getDepthFormat() {
    return depth_format_;
}

VkExtent2D VulkanSwapchain::getExtent() {
    return swap_chain_extent_;
}

uint32_t VulkanSwapchain::getImageCount() {
    return swap_chain_images_.size();
}

VkImageView VulkanSwapchain::getImageView(size_t index) {
    return swap_chain_image_views_[index];

}

VkImageView VulkanSwapchain::getDepthImageView() {
    return depth_image_view_;
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice* device, VkSwapchainKHR swap_chain, VkExtent2D extent, VkFormat format, std::vector<VkImage>& swap_chain_images)
: device_(device), swap_chain_(swap_chain), image_format_(format), swap_chain_extent_(extent) {
    swap_chain_images_.swap(swap_chain_images);
    createImageViews();
    createDepthResources();
}

VulkanSwapchain::~VulkanSwapchain() {
    vkDestroyImageView(*device_, depth_image_view_, nullptr);
    vkDestroyImage(*device_, depth_image_, nullptr);
    vkFreeMemory(*device_, depth_image_memory_, nullptr);

    for (auto& image_view : swap_chain_image_views_) {
        vkDestroyImageView(*device_, image_view, nullptr);
    }
    for (auto& image : swap_chain_images_) {
        vkDestroyImage(*device_, image, nullptr);
    }
    vkDestroySwapchainKHR(*device_, swap_chain_, nullptr);
}

void VulkanSwapchain::createImageViews() {
    swap_chain_image_views_.resize(swap_chain_images_.size());
    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
        swap_chain_image_views_[i] = createImageView(swap_chain_images_[i], image_format_, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}


VkImageView VulkanSwapchain::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    VK_CHECK_RESULT(vkCreateImageView(*device_, &view_info, nullptr, &image_view));

    return image_view;
}

VkFormat VulkanSwapchain::findDepthFormat() {
    return device_->findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VulkanSwapchain::createDepthResources() {
    VkFormat depth_format = findDepthFormat();
    depth_format_ = depth_format;
    device_->createImage(swap_chain_extent_.width, swap_chain_extent_.height, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image_, depth_image_memory_);
    depth_image_view_ = createImageView(depth_image_, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkCommandBuffer command_buffer = device_->beginCommandBuffer();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = depth_image_;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT || depth_format == VK_FORMAT_D24_UNORM_S8_UINT) {
        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkPipelineStageFlags    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    vkCmdPipelineBarrier(command_buffer, source_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    device_->submitCommandBuffer(command_buffer, device_->getGraphicsQueue());
}
