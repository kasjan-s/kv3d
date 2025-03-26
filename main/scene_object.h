#pragma once

#include <memory>

#include "main/camera.h"
#include "main/material.h"
#include "main/model.h"
#include "main/texture.h"
#include "main/vulkan_device.h"

class Texture;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct SceneObjectPushConstant {
    alignas(16) glm::vec3 light_ambient = glm::vec3(1.0f, 1.0f, 1.0f);
    alignas(16) glm::vec3 light_diffuse = glm::vec3(1.0f, 1.0f, 1.0f);
    alignas(16) glm::vec3 light_specular = glm::vec3(1.0f, 1.0f, 1.0f);
    alignas(16) glm::vec3 light_pos_;
    alignas(16) glm::vec3 camera_pos_;
    alignas(16) glm::vec3 ambient = glm::vec3(1.0f);
    alignas(16) glm::vec3 diffuse = glm::vec3(1.0f);
    alignas(16) glm::vec3 specular = glm::vec3(1.0f);
    alignas(4) float shininess = 32.0f;
    alignas(4) VkBool32 is_textured_;
};

class SceneObject {
public:
    SceneObject(VulkanDevice* device);
    SceneObject(SceneObject&&) = default;
    SceneObject(const SceneObject&) = delete;
    SceneObject& operator =(const SceneObject&) = delete;
    ~SceneObject();
    
    void loadModel(const std::string& model_path);
    void loadTexture(const std::string& texture_path);
    void setMaterial(MaterialType material);
    void draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, uint32_t image_index, const glm::vec3& camera_position);
    UniformBufferObject getMatrices();
    void createUniformBuffers(int buffer_count);
    void updateUniformBuffer(uint32_t image_index, const Camera& camera);
    void createDescriptorSets(VkDescriptorSetLayout descriptor_set_layout);
    void setPos(const glm::vec3& pos);
    SceneObjectPushConstant getPushConstants() const;

private:
    void createDescriptorPool(VkDescriptorSetLayout descriptor_set_layout);
    void updateDescriptorSets();
    UniformBufferObject matrices_;
    std::unique_ptr<Model> model_;
    VulkanDevice* device_;
    std::vector<VkBuffer> uniform_buffers_;
    std::vector<VkDeviceMemory> uniform_buffers_memory_;
    std::vector<void*> uniform_buffers_mapped_;
    std::vector<VkDescriptorSet> descriptor_sets_;
    VkDescriptorPool descriptor_pool_;
    glm::vec3 pos_;
    std::unique_ptr<Texture> texture_;
    SceneObjectPushConstant push_constants_;
};