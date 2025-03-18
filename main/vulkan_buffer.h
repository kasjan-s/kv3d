#pragma once

#include "vulkan/vulkan.h"

#include <cstring>

struct Buffer {
    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDescriptorBufferInfo descriptor;
    VkDeviceSize size = 0;
    VkDeviceSize alignment_= 0;
    void* mapped = nullptr;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkMemoryPropertyFlags property_flags;
    VkBufferUsageFlags usage_flags;

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
        return vkMapMemory(device, memory, offset, size, 0, &mapped);
    }

    void unmap() {
        if (mapped) {
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }
    }

    VkResult bind(VkDeviceSize offset = 0) {
        return vkBindBufferMemory(device, buffer, memory, offset);
    }

    void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
        descriptor.offset = offset;
        descriptor.buffer = buffer;
        descriptor.range = size;
    }

    void copyTo(void* data, VkDeviceSize size) {
        std::memcpy(mapped, data, size);
    }

    void destroy() {
        if (buffer) {
            vkDestroyBuffer(device, buffer, nullptr);
        }
        if (memory) {
            vkFreeMemory(device, memory, nullptr);
        }
    }
};