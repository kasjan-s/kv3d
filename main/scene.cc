#include "main/scene.h"

void Scene::createObject(VulkanDevice* device, const std::string& model_path, const std::string& texture_path, glm::vec3 pos, uint32_t frames) {
    std::unique_ptr<SceneObject> object = std::make_unique<SceneObject>(device);
    object->loadModel(model_path);
    object->loadTexture(texture_path);
    object->createUniformBuffers(frames);
    object->setPos(pos);
    scene_objects_.push_back(object.get());
    objects_container_.insert(std::move(object));
}

void Scene::createObject(VulkanDevice* device, const std::string& model_path, MaterialType material, glm::vec3 pos, uint32_t frames) {
    std::unique_ptr<SceneObject> object = std::make_unique<SceneObject>(device);
    object->loadModel(model_path);
    object->setMaterial(material);
    object->createUniformBuffers(frames);
    object->setPos(pos);
    scene_objects_.push_back(object.get());
    objects_container_.insert(std::move(object));
}


void Scene::clear() {
    scene_objects_.clear();
    objects_container_.clear();
}

void Scene::createDescriptorSets(VkDescriptorSetLayout descriptor_set_layout) {
    for (auto& obj : scene_objects_) {
        obj->createDescriptorSets(descriptor_set_layout);
    }
}

void Scene::draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, uint32_t image_index) {
    for (auto& object : scene_objects_) {
        object->draw(command_buffer, pipeline_layout, image_index, camera_.getPosition());
    }
}

void Scene::updateUniformBuffers(uint32_t image_index) {
    for (auto& obj : scene_objects_) {
        obj->updateUniformBuffer(image_index, camera_);
    }
}

void Scene::setScreenSize(size_t width, size_t height) {
    width_ = width;
    height_ = height;
    camera_.setScreenSize(width, height);
}

void Scene::moveCamera(float dx, float dy) {
    camera_.move(dx, dy);
}

void Scene::rotateCamera(float dx, float dy) {
    camera_.rotateBy(dx, dy);
}