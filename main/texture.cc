#include "main/texture.h"

#include "main/vulkan_buffer.h"
#include "main/vulkan_device.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include <iostream>
#include <stdexcept>


std::unique_ptr<Texture> Texture::createFromFile(const std::string &file, VulkanDevice* device) {
    int tex_width, tex_height, tex_channels;
    stbi_uc* pixels = stbi_load(file.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("Failed to load texture image!");
    }

    Buffer staging_buffer;
    staging_buffer.size = tex_width * tex_height * 4;
    staging_buffer.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    staging_buffer.usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer.device = *device;
    device->createBuffer(staging_buffer);

    void* data;
    staging_buffer.map();
    staging_buffer.copyTo(pixels, staging_buffer.size);
    staging_buffer.unmap();
    stbi_image_free(pixels);

    auto texture = std::unique_ptr<Texture>(new Texture(*device));
    texture->initImage(device, tex_width, tex_height, staging_buffer);
    texture->initSampler(device);
    
    staging_buffer.destroy();

    return texture;
}

Texture::~Texture() {
    vkDestroySampler(device_, sampler_, nullptr);
    vkDestroyImageView(device_, texture_image_view_, nullptr);
    vkDestroyImage(device_, texture_image_, nullptr);
    vkFreeMemory(device_, texture_image_memory_, nullptr);
}

Texture::Texture(VkDevice device) : device_(device) {}

void Texture::initImage(VulkanDevice* device, uint32_t width, uint32_t height, Buffer& staging_buffer) {
    device->createImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image_, texture_image_memory_);
    VkCommandBuffer command_buffer = device->beginCommandBuffer();
    transitionImageLayoutCmd(command_buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImageCmd(command_buffer, width, height, staging_buffer);
    transitionImageLayoutCmd(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    device->submitCommandBuffer(command_buffer, device->getGraphicsQueue());

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = texture_image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    VK_CHECK_RESULT(vkCreateImageView(*device, &view_info, nullptr, &texture_image_view_));
    descriptor_.imageView = texture_image_view_;
    descriptor_.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void Texture::initSampler(VulkanDevice* device) {
    VkSamplerCreateInfo sampler_create_info = {};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = VK_FILTER_LINEAR;
    sampler_create_info.minFilter = VK_FILTER_LINEAR;
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.compareOp = VK_COMPARE_OP_NEVER;
    sampler_create_info.minLod = 0.0f;
    sampler_create_info.maxLod = 0.0f;

    sampler_create_info.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device->getPhysicalDevice(), &properties);
    sampler_create_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(*device, &sampler_create_info, nullptr, &sampler_));
    
    descriptor_.sampler = sampler_;
}

void Texture::transitionImageLayoutCmd(VkCommandBuffer command_buffer, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture_image_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    vkCmdPipelineBarrier(command_buffer, source_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Texture::copyBufferToImageCmd(VkCommandBuffer command_buffer, uint32_t width, uint32_t height, Buffer& staging_buffer) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width, height, 1
    };
    vkCmdCopyBufferToImage(command_buffer, staging_buffer.buffer, texture_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

VkDescriptorImageInfo* Texture::getDescriptor() {
    return &descriptor_;
}