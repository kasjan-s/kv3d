#pragma once

#include <memory>
#include <string>
#include <vector>

#include "main/vertex.h"
#include "main/vulkan_buffer.h"

class VulkanDevice;

class Model {
public:
    static std::unique_ptr<Model> loadFromFile(const std::string& file, VulkanDevice* device);
    ~Model();

    void draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, VkDescriptorSet& descriptor_set);
private:
    Model() = default;
    Model(VulkanDevice* device, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

    void createVertexBuffer(std::vector<Vertex>& vertices, VulkanDevice* device);
    void createIndexBuffer(std::vector<uint32_t>& indices, VulkanDevice* device);

    Buffer index_buffer_;
    Buffer vertex_buffer_;
    VkDescriptorSet descriptor_set_;
    uint32_t indices_count_;
};
