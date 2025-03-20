#pragma once

#include "main/scene_object.h"

#include <unordered_set>
#include <vector>

class Scene {
public:
    void createObject(VulkanDevice* device, const std::string& model_path, const std::string& texture_path, glm::vec3 pos, uint32_t frames = kMaxFramesInFlight);
    void clear();
    void createDescriptorSets(VkDescriptorSetLayout descriptor_set_layout);
    void draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, uint32_t image_index);
    void updateUniformBuffers(uint32_t image_index, VkExtent2D extent);

private:
    std::unordered_set<std::unique_ptr<SceneObject>> objects_container_;
    std::vector<SceneObject*> scene_objects_;
};