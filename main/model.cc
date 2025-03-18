#include "main/model.h"

#include "main/vulkan_buffer.h"
#include "main/vertex.h"
#include "main/vulkan_device.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tiny_obj_loader.h"

#include <iostream>

std::unique_ptr<Model> Model::loadFromFile(const std::string& file, VulkanDevice* device) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file.c_str())) {
        throw std::runtime_error(warn + err);
    }

    // std::vector<uint32_t> indices;
    // std::vector<Vertex> vertices;
    // std::unordered_map<Vertex, uint32_t> unique_vertices;

    // for (const auto& shape : shapes) {
    //     for (const auto& index : shape.mesh.indices) {
    //         Vertex vertex{};

    //         vertex.pos = {
    //             attrib.vertices[3 * index.vertex_index + 0],
    //             attrib.vertices[3 * index.vertex_index + 1],
    //             attrib.vertices[3 * index.vertex_index + 2]
    //         };

    //         vertex.tex_coords = {
    //             attrib.texcoords[2 * index.texcoord_index + 0],
    //             1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
    //         };

    //         vertex.color = {1.0f, 1.0f, 1.0f};

    //         if (unique_vertices.count(vertex) == 0) {
    //             unique_vertices[vertex] = vertices.size();
    //             vertices.push_back(vertex);
    //         }

    //         indices.push_back(unique_vertices[vertex]);
    //     }
    // }
    
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };
    
    std::unique_ptr<Model> model(new Model(device, vertices, indices));
    return model;
}

Model::Model(VulkanDevice* device, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    createVertexBuffer(vertices, device);
    createIndexBuffer(indices, device);
    // createDescriptorSet();
}

void Model::createVertexBuffer(std::vector<Vertex>& vertices, VulkanDevice* device) {
    Buffer staging_buffer;
    staging_buffer.size = sizeof(vertices[0]) * vertices.size();
    staging_buffer.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    staging_buffer.usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer.device = *device;
    device->createBuffer(staging_buffer);

    staging_buffer.map();
    staging_buffer.copyTo(vertices.data(), staging_buffer.size);
    staging_buffer.unmap();

    vertex_buffer_.size = sizeof(vertices[0]) * vertices.size();
    vertex_buffer_.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vertex_buffer_.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertex_buffer_.device = *device;
    device->createBuffer(vertex_buffer_);

    VkCommandBuffer command_buffer = device->beginCommandBuffer();

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = staging_buffer.size;
    vkCmdCopyBuffer(command_buffer, staging_buffer.buffer, vertex_buffer_.buffer, 1, &copy_region);
    device->submitCommandBuffer(command_buffer, device->getGraphicsQueue());

    staging_buffer.destroy();
}

void Model::createIndexBuffer(std::vector<uint32_t>& indices, VulkanDevice* device) {
    Buffer staging_buffer;
    staging_buffer.size = sizeof(indices[0]) * indices.size();
    staging_buffer.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    staging_buffer.usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer.device = *device;
    device->createBuffer(staging_buffer);

    void* data;
    staging_buffer.map();
    staging_buffer.copyTo(indices.data(), staging_buffer.size);
    staging_buffer.unmap();

    index_buffer_.size = sizeof(indices[0]) * indices.size();
    index_buffer_.property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    index_buffer_.usage_flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    index_buffer_.device = *device;
    device->createBuffer(index_buffer_);

    VkCommandBuffer command_buffer = device->beginCommandBuffer();
    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = staging_buffer.size;
    vkCmdCopyBuffer(command_buffer, staging_buffer.buffer, index_buffer_.buffer, 1, &copy_region);
    device->submitCommandBuffer(command_buffer, device->getGraphicsQueue());

    staging_buffer.destroy();
}

void Model::draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout) {
    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_.buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, index_buffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
}

Model::~Model() {
    index_buffer_.destroy();
    vertex_buffer_.destroy();
}