#include <cstdint>
#include <limits>
#include <string_view>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "main/vulkan_device.h"
#include "main/model.h"
#include "main/vulkan_swapchain.h"
#include "main/texture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <cstdlib>
#include <set>
#include <string_view>
#include <vector>
#include <filesystem>

#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

bool checkValidationLayerSupport() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : kValidationLayers) {
        bool layer_found = false;

        for (const auto& layer_properties : available_layers) {
            if (*layer_name == *layer_properties.layerName) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found)
            return false;
    }
    
    return true;
}


constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

const std::string MODEL_PATH = "main/models/viking_room.obj";
const std::string TEXTURE_PATH = "main/textures/viking_room.png";

class HelloTriangleApplication {
public:
    explicit HelloTriangleApplication(Runfiles* runfiles)
        : runfiles_(runfiles) {}

    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    std::vector<char> readFile(const std::string& filename) {
        std::string file_path = runfiles_->Rlocation("_main/" + filename);

        std::ifstream file(file_path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file!");
        }

        size_t file_size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);
        file.close();

        return buffer;
    }

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        
        glfwSetWindowUserPointer(window_, this);
        auto callback = [](GLFWwindow* window, int width, int height) {
            static_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window))->framebufferResizeCallback(width, height);
        };
        glfwSetFramebufferSizeCallback(window_, callback);
    }

    void framebufferResizeCallback(int width, int height) {
        framebuffer_resized_ = true;
    }

    void createInstance() {
        if (kEnableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        app_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<VkExtensionProperties> available_extensions = getAvailableExtensions();
        for (int i = 0; i < glfwExtensionCount; ++i) {
            std::string_view extension_name(glfwExtensions[i]);
            bool found = false;
            for (const auto& extension : available_extensions) {
                if (extension.extensionName == extension_name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Required extension is missing! " + std::string(extension_name));
            }
        }

        create_info.enabledExtensionCount = glfwExtensionCount;
        create_info.ppEnabledExtensionNames = glfwExtensions;

        if (kEnableValidationLayers) {
            create_info.enabledLayerCount = static_cast<uint32_t>(std::size(kValidationLayers));
            create_info.ppEnabledLayerNames = kValidationLayers;
        } else {
            create_info.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VK instance!");
        }
    }

    std::vector<VkExtensionProperties> getAvailableExtensions() {
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
        return extensions;
    }

    void initVulkan() {
        createInstance();
        createSurface();
        vulkan_device_ = std::make_unique<VulkanDevice>(instance_, surface_);
        swapchain_ = VulkanSwapchain::createSwapChain(vulkan_device_.get(), surface_, window_);

        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createTexture();

        model_ = Model::loadFromFile(MODEL_PATH, vulkan_device_.get());

        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding ubo_layout_binding{};
        ubo_layout_binding.binding = 0;
        ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout_binding.descriptorCount = 1;
        ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        ubo_layout_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding sampler_layout_binding{};
        sampler_layout_binding.binding = 1;
        sampler_layout_binding.descriptorCount = 1;
        sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_layout_binding.pImmutableSamplers = nullptr;
        sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {ubo_layout_binding, sampler_layout_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = bindings.size();
        layout_info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(*vulkan_device_, &layout_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
        return vulkan_device_->createBuffer(size, usage, properties, buffer, buffer_memory);
    }

    VkFormat findDepthFormat() {
        return vulkan_device_->findSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void createTexture() {
        texture_ = Texture::createFromFile(TEXTURE_PATH.c_str(), vulkan_device_.get());
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView image_view;
        if (vkCreateImageView(*vulkan_device_, &view_info, nullptr, &image_view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }

        return image_view;
    }

    void createUniformBuffers() {
        VkDeviceSize buffer_size = sizeof(UniformBufferObject);

        uniform_buffers_.resize(kMaxFramesInFlight);
        uniform_buffers_memory_.resize(kMaxFramesInFlight);
        uniform_buffers_mapped_.resize(kMaxFramesInFlight);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            createBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniform_buffers_[i], uniform_buffers_memory_[i]);

            vkMapMemory(*vulkan_device_, uniform_buffers_memory_[i], 0, buffer_size, 0, &uniform_buffers_mapped_[i]);
        }
    }

    void createDescriptorPool() {
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

        if (vkCreateDescriptorPool(*vulkan_device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(kMaxFramesInFlight, descriptor_set_layout_);
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptor_pool_;
        alloc_info.descriptorSetCount = static_cast<uint32_t>(kMaxFramesInFlight);
        alloc_info.pSetLayouts = layouts.data();

        descriptor_sets_.resize(kMaxFramesInFlight);
        if (vkAllocateDescriptorSets(*vulkan_device_, &alloc_info, descriptor_sets_.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

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
                descriptor_write.pImageInfo = texture_->getDescriptor();
                descriptor_write.pNext = nullptr;
                descriptor_write.pBufferInfo = VK_NULL_HANDLE;
                descriptor_writes[1] = descriptor_write;
            }

            vkUpdateDescriptorSets(*vulkan_device_, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
        }
    }

    void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
        VkCommandBuffer command_buffer = vulkan_device_->beginCommandBuffer();

        VkBufferCopy copy_region{};
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

        vulkan_device_->submitCommandBuffer(command_buffer, vulkan_device_->getGraphicsQueue());
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
        VkCommandBuffer command_buffer = vulkan_device_->beginCommandBuffer();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        
        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dst_stage;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(command_buffer, source_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vulkan_device_->submitCommandBuffer(command_buffer, vulkan_device_->getGraphicsQueue());
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer command_buffer = vulkan_device_->beginCommandBuffer();

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

        vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        vulkan_device_->submitCommandBuffer(command_buffer, vulkan_device_->getGraphicsQueue());
    }

    void createSyncObjects() {
        image_available_semaphores_.resize(kMaxFramesInFlight);
        render_finished_semaphores_.resize(kMaxFramesInFlight);
        in_flight_fences_.resize(kMaxFramesInFlight);

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            if (vkCreateSemaphore(*vulkan_device_, &semaphore_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(*vulkan_device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(*vulkan_device_, &fence_info, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create semaphores!");
            }
        }
    }

    void recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index) {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkExtent2D extent = swapchain_->getExtent();
        VkRenderPassBeginInfo render_pass_info;
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass_;
        render_pass_info.framebuffer = swap_chain_framebuffers_[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = extent;

        VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkClearValue clear_depth{};
        clear_depth.depthStencil = {1.0f, 0};
        std::array<VkClearValue, 2> clear_values = {clear_color, clear_depth};
        render_pass_info.clearValueCount = clear_values.size();
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        model_->draw(command_buffer, pipeline_layout_, descriptor_sets_[current_frame_]);

        vkCmdEndRenderPass(command_buffers_[current_frame_]);
        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    void createCommandBuffers() {
        command_buffers_.resize(kMaxFramesInFlight);
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = vulkan_device_->getCommandPool();
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = command_buffers_.size();

        if (vkAllocateCommandBuffers(*vulkan_device_, &alloc_info, command_buffers_.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }

    void createFramebuffers() {
        size_t image_count = swapchain_->getImageCount();
        swap_chain_framebuffers_.resize(image_count);

        for (size_t i = 0; i < image_count; ++i) {
            std::vector<VkImageView> attachments = {
                swapchain_->getImageView(i),
                swapchain_->getDepthImageView()
            };

            VkExtent2D extent = swapchain_->getExtent();
            VkFramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = render_pass_;
            framebuffer_info.attachmentCount = attachments.size();
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = extent.width;
            framebuffer_info.height = extent.height;
            framebuffer_info.layers = 1;

            if (vkCreateFramebuffer(*vulkan_device_, &framebuffer_info, nullptr, &swap_chain_framebuffers_[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }
    
    void createRenderPass() {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = swapchain_->getImageFormat();
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depth_attachment{};
        depth_attachment.format = findDepthFormat();
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_attachment_ref{};
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;



        VkSubpassDependency depdency{};
        depdency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depdency.dstSubpass = 0;
        depdency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        depdency.srcAccessMask = 0;
        depdency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        depdency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = attachments.size();
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &depdency;

        if (vkCreateRenderPass(*vulkan_device_, &render_pass_info, nullptr, &render_pass_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void createGraphicsPipeline() {
        auto vert_shader_code = readFile("main/shaders/shader.vert.spv");
        auto frag_shader_code = readFile("main/shaders/shader.frag.spv");

        VkShaderModule vert_shader_module = createShaderModule(vert_shader_code);
        VkShaderModule frag_shader_module = createShaderModule(frag_shader_code);

        VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
        vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_stage_info.module = vert_shader_module;
        vert_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
        frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module = frag_shader_module;
        frag_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 1;
        auto binding_description = Vertex::getBindingDescription();
        vertex_input_info.pVertexBindingDescriptions = &binding_description;
        auto attribute_description = Vertex::getAttributeDescriptions();
        vertex_input_info.vertexAttributeDescriptionCount = attribute_description.size();
        vertex_input_info.pVertexAttributeDescriptions = attribute_description.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkExtent2D extent = swapchain_->getExtent();
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;

        std::vector<VkDynamicState> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_TRUE;
        color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;
        color_blending.blendConstants[0] = 0.0f; // Optional
        color_blending.blendConstants[1] = 0.0f; // Optional
        color_blending.blendConstants[2] = 0.0f; // Optional
        color_blending.blendConstants[3] = 0.0f; // Optional

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(*vulkan_device_, &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = pipeline_layout_;
        pipeline_info.renderPass = render_pass_;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(*vulkan_device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(*vulkan_device_, frag_shader_module, nullptr);
        vkDestroyShaderModule(*vulkan_device_, vert_shader_module, nullptr);
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module;
        if (vkCreateShaderModule(*vulkan_device_, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }

        return shader_module;
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }
        
        vkDeviceWaitIdle(*vulkan_device_);

        swapchain_ = VulkanSwapchain::createSwapChain(vulkan_device_.get(), surface_, window_);

        createFramebuffers();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        for (int i = 0; i < queue_family_count; ++i) {
            if (indices.isComplete())
                break;

            VkBool32 presentation_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentation_support);
            if (presentation_support) {
                indices.presentation_family = i;
            }

            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics_family = i;
            }            
        }


        return indices;
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(*vulkan_device_);
    }

    void drawFrame() {
        vkWaitForFences(*vulkan_device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(*vulkan_device_, *swapchain_, UINT64_MAX, image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        vkResetFences(*vulkan_device_, 1, &in_flight_fences_[current_frame_]);

        vkResetCommandBuffer(command_buffers_[current_frame_], 0);
        recordCommandBuffer(command_buffers_[current_frame_], image_index);

        updateUniformBuffer(current_frame_);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = {image_available_semaphores_[current_frame_]};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers_[current_frame_];
        VkSemaphore signal_semaphores[] = {render_finished_semaphores_[current_frame_]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        if (vkQueueSubmit(vulkan_device_->getGraphicsQueue(), 1, &submit_info, in_flight_fences_[current_frame_]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swap_chains[] = {*swapchain_};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr;

        result = vkQueuePresentKHR(vulkan_device_->getPresentationQueue(), &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
            framebuffer_resized_ = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
    }

    void updateUniformBuffer(uint32_t current_image) {
        static auto s_start_time = std::chrono::high_resolution_clock::now();

        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - s_start_time).count();

        VkExtent2D extent = swapchain_->getExtent();
        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), extent.width / static_cast<float>(extent.height), 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        std::memcpy(uniform_buffers_mapped_[current_image], &ubo, sizeof(ubo));
    }

    void cleanup() {
        swapchain_.reset();

        for (auto framebuffer : swap_chain_framebuffers_) {
            vkDestroyFramebuffer(*vulkan_device_, framebuffer, nullptr);
        }

        texture_.reset();

        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            vkDestroyBuffer(*vulkan_device_, uniform_buffers_[i], nullptr);
            vkFreeMemory(*vulkan_device_, uniform_buffers_memory_[i], nullptr);
        }

        vkDestroyDescriptorPool(*vulkan_device_, descriptor_pool_, nullptr);
        vkDestroyDescriptorSetLayout(*vulkan_device_, descriptor_set_layout_, nullptr);

        model_.reset();

        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            vkDestroySemaphore(*vulkan_device_, image_available_semaphores_[i], nullptr);
            vkDestroySemaphore(*vulkan_device_, render_finished_semaphores_[i], nullptr);
            vkDestroyFence(*vulkan_device_, in_flight_fences_[i], nullptr);
        }

        vkDestroyPipeline(*vulkan_device_, graphics_pipeline_, nullptr);
        vkDestroyPipelineLayout(*vulkan_device_, pipeline_layout_, nullptr);
        vkDestroyRenderPass(*vulkan_device_, render_pass_, nullptr);
        
        vulkan_device_.reset();
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);

        glfwDestroyWindow(window_);
        
        glfwTerminate();
    }

    std::vector<VkCommandBuffer> command_buffers_;
    uint32_t current_frame_ = 0;
    VkDescriptorPool descriptor_pool_;
    std::vector<VkDescriptorSet> descriptor_sets_;
    std::unique_ptr<VulkanDevice> vulkan_device_;
    VkDescriptorSetLayout descriptor_set_layout_;
    bool framebuffer_resized_ = false;
    VkPipeline graphics_pipeline_;
    std::vector<VkFence> in_flight_fences_;
    VkInstance instance_;
    std::vector<VkSemaphore> image_available_semaphores_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    VkRenderPass render_pass_;
    Runfiles* runfiles_;
    VkSurfaceKHR surface_;
    std::unique_ptr<VulkanSwapchain> swapchain_;
    std::vector<VkFramebuffer> swap_chain_framebuffers_;
    std::unique_ptr<Texture> texture_;
    std::unique_ptr<Model> model_;
    std::vector<VkBuffer> uniform_buffers_;
    std::vector<VkDeviceMemory> uniform_buffers_memory_;
    std::vector<void*> uniform_buffers_mapped_;

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    
    GLFWwindow* window_;
};

int main(int argc, char** argv) {
    std::string error;
    std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], &error));

    if (runfiles == nullptr) {
        std::cerr << error << std::endl;
        return EXIT_FAILURE;
    }

    HelloTriangleApplication app(runfiles.get());

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}