
#include "main/scene_object.h"

#include <chrono>

#include "main/texture.h"

SceneObject::SceneObject(VulkanDevice* device) 
    : device_(device) {}

void SceneObject::loadModel(const std::string& model_path) {
    model_ = Model::loadFromFile(model_path, device_);
}

void SceneObject::draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout, uint32_t image_index) {
    model_->draw(command_buffer, pipeline_layout, descriptor_sets_[image_index]);
}

UniformBufferObject SceneObject::getMatrices() {
    return matrices_;
}

SceneObject::~SceneObject() {
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

void SceneObject::updateUniformBuffer(uint32_t image_index, VkExtent2D extent) {
    static auto s_start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - s_start_time).count();

    UniformBufferObject ubo{};
    ubo.model = glm::translate(glm::mat4(1.0f), pos_) * glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(80.0f, 4.0f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), extent.width / static_cast<float>(extent.height), 0.1f, 200.0f);
    ubo.proj[1][1] *= -1;

    std::memcpy(uniform_buffers_mapped_[image_index], &ubo, sizeof(ubo));
}

void SceneObject::setPos(float i) {
    pos_ = i * glm::vec3(0.0f, 0.0f, 1.0f);
}

void SceneObject::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> pool_sizes{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight);
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = static_cast<uint32_t>(kMaxFramesInFlight);

    if (vkCreateDescriptorPool(*device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void SceneObject::createDescriptorSets(VkDescriptorSetLayout descriptor_set_layout, Texture* texture) {
    createDescriptorPool();
    std::vector<VkDescriptorSetLayout> layouts(kMaxFramesInFlight, descriptor_set_layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(kMaxFramesInFlight);
    alloc_info.pSetLayouts = layouts.data();

    descriptor_sets_.resize(kMaxFramesInFlight);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device_, &alloc_info, descriptor_sets_.data()));

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = uniform_buffers_[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        std::array<VkWriteDescriptorSet, 2> descriptor_writes{};
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
            descriptor_writes[0] = descriptor_write;
        }
        {
            // Image sampler
            VkWriteDescriptorSet descriptor_write;
            descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet = descriptor_sets_[i];
            descriptor_write.dstBinding = 1;
            descriptor_write.dstArrayElement = 0;
            descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_write.descriptorCount = 1;
            descriptor_write.pImageInfo = texture->getDescriptor();
            descriptor_write.pNext = nullptr;
            descriptor_write.pBufferInfo = VK_NULL_HANDLE;
            descriptor_writes[1] = descriptor_write;
        }

        vkUpdateDescriptorSets(*device_, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
}