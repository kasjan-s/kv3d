#pragma once

#include "main/vulkan_buffer.h"
#include "vulkan/vulkan.h"

#include <memory>
#include <string>

class VulkanDevice;

class Texture {
public:
    static std::unique_ptr<Texture> createFromFile(const std::string& file, VulkanDevice* device);

    VkDescriptorImageInfo* getDescriptor();

private:
    Texture() = default;
    void initImage(VulkanDevice* device, uint32_t width, uint32_t height, Buffer& staging_buffer);
    void initSampler(VulkanDevice* device);
    void transitionImageLayoutCmd(VkCommandBuffer command_buffer, VkImageLayout old_layout, VkImageLayout new_layout);
    void copyBufferToImageCmd(VkCommandBuffer command_buffer, uint32_t width, uint32_t height, Buffer& staging_buffer);

    VkImage texture_image_;
    VkDeviceMemory texture_image_memory_;
    VkImageView texture_image_view_;
    VkDescriptorImageInfo descriptor_;
    VkSampler sampler_;
};