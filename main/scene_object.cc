
#include "main/scene_object.h"

#include <chrono>

#include "main/texture.h"

SceneObject::SceneObject(VulkanDevice* device) 
    : device_(device) {}

void SceneObject::loadModel(const std::string& model_path) {
    model_ = Model::loadFromFile(model_path, device_);
}

void SceneObject::loadTexture(const std::string& texture_path) {
    if (texture_path.empty()) {
        push_constants_.is_textured_ = VK_FALSE;
    } else {
        push_constants_.is_textured_ = VK_TRUE;
        texture_ = Texture::createFromFile(texture_path.c_str(), device_);
    }
}

void SceneObject::setMaterial(MaterialType material_type) {
    std::unique_ptr<Material> material = getMaterial(material_type);
    push_constants_.is_textured_ = VK_FALSE;
    push_constants_.ambient = material->ambient();
    push_constants_.diffuse = material->diffuse();
    push_constants_.specular = material->specular();
    push_constants_.shininess = material->shininess();
}

void SceneObject::draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, uint32_t image_index, const glm::vec3& camera_position) {
    static auto s_start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - s_start_time).count();

    updateDescriptorSets();
    push_constants_.light_pos_ = glm::vec3(std::sin(time) * 200.0f, 200.f, std::cos(time) * 200.0f);
    push_constants_.camera_pos_ = camera_position;
    vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SceneObjectPushConstant), &push_constants_);
    model_->draw(command_buffer, pipeline_layout, descriptor_sets_[image_index]);
}

UniformBufferObject SceneObject::getMatrices() {
    return matrices_;
}

SceneObject::~SceneObject() {
    texture_.reset();
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        vkDestroyBuffer(*device_, uniform_buffers_[i], nullptr);
        vkFreeMemory(*device_, uniform_buffers_memory_[i], nullptr);
    }
    vkDestroyDescriptorPool(*device_, descriptor_pool_, nullptr);
}

void SceneObject::createUniformBuffers(int buffer_count) {
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    uniform_buffers_.resize(buffer_count);
    uniform_buffers_memory_.resize(buffer_count);
    uniform_buffers_mapped_.resize(buffer_count);

    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
        device_->createBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniform_buffers_[i], uniform_buffers_memory_[i]);

        vkMapMemory(*device_, uniform_buffers_memory_[i], 0, buffer_size, 0, &uniform_buffers_mapped_[i]);
    }
}

void SceneObject::updateUniformBuffer(uint32_t image_index, const Camera& camera) {
    static auto s_start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - s_start_time).count();

    UniformBufferObject ubo{};
    ubo.model = glm::translate(glm::mat4(1.0f), pos_) /* glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f))*/;
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getPerspectiveMatrix();

    std::memcpy(uniform_buffers_mapped_[image_index], &ubo, sizeof(ubo));
}

void SceneObject::setPos(const glm::vec3& pos) {
    pos_ = pos;
}

void SceneObject::createDescriptorPool(VkDescriptorSetLayout descriptor_set_layout) {
    std::vector<VkDescriptorPoolSize> pool_sizes{};
    VkDescriptorPoolSize uniform_buffer_descriptor;
    uniform_buffer_descriptor.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniform_buffer_descriptor.descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight);
    pool_sizes.push_back(uniform_buffer_descriptor);
    if (texture_) {
        VkDescriptorPoolSize texture_sampler;
        texture_sampler.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texture_sampler.descriptorCount = kMaxFramesInFlight;
        pool_sizes.push_back(texture_sampler);
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(kMaxFramesInFlight);

    if (vkCreateDescriptorPool(*device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
    
    std::vector<VkDescriptorSetLayout> layouts(kMaxFramesInFlight, descriptor_set_layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(kMaxFramesInFlight);
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets_.resize(kMaxFramesInFlight);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device_, &alloc_info, descriptor_sets_.data()));
}

void SceneObject::createDescriptorSets(VkDescriptorSetLayout descriptor_set_layout) {
    createDescriptorPool(descriptor_set_layout);
    updateDescriptorSets();
}

void SceneObject::updateDescriptorSets() {
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = uniform_buffers_[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        std::vector<VkWriteDescriptorSet> descriptor_writes{};
        {
            // Uniform Buffer
            VkWriteDescriptorSet descriptor_write;
            descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet = descriptor_sets_[i];
            descriptor_write.dstBinding = 0;
            descriptor_write.dstArrayElement = 0;
            descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_write.descriptorCount = 1;
            descriptor_write.pImageInfo = VK_NULL_HANDLE;
            descriptor_write.pNext = nullptr;
            descriptor_write.pBufferInfo = &buffer_info;
            descriptor_writes.push_back(descriptor_write);
        }
        if (texture_) {
            // Image sampler
            VkWriteDescriptorSet descriptor_write;
            descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet = descriptor_sets_[i];
            descriptor_write.dstBinding = 1;
            descriptor_write.dstArrayElement = 0;
            descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_write.descriptorCount = 1;
            descriptor_write.pImageInfo = texture_->getDescriptor();
            descriptor_write.pNext = nullptr;
            descriptor_write.pBufferInfo = VK_NULL_HANDLE;
            descriptor_writes.push_back(descriptor_write);
        } 

        vkUpdateDescriptorSets(*device_, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
}

SceneObjectPushConstant SceneObject::getPushConstants() const {
    return push_constants_;
}