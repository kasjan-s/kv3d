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

#include <array>
#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <cstdlib>
#include <set>
#include <string_view>
#include <vector>

#include "main/model.h"
#include "main/texture.h"
#include "main/vertex.h"
#include "main/vulkan_buffer.h"
#include "main/vulkan_constants.h"
#include "main/vulkan_device.h"
#include "main/vulkan_swapchain.h"
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

class Sphere {
public:
    VkDescriptorSet descriptor_set_;
    
    struct Matrices {
        glm::mat4 projection_;
        glm::mat4 view_;
        glm::mat4 model_;
    } matrices;

    glm::vec3 rotation_{0.0f};

    Buffer uniform_buffer_;
    std::unique_ptr<Texture> texture_;
    std::unique_ptr<Model> model_;

    VkDescriptorSet* getDescriptorSet() {
        return &descriptor_set_;
    }

    void loadAssets(VulkanDevice* device) {
        loadModel(device);
        loadTexture(device);
    }

    void loadModel(VulkanDevice* device) {
        model_ = Model::loadFromFile("main/models/sphere.obj", device);
    }

    void loadTexture(VulkanDevice* device) {
        texture_ = Texture::createFromFile("main/textures/Blue_Marble_002_COLOR.png", device);
    }

    void setupDescriptor(VkDevice device, VkDescriptorPool descriptor_pool, VkDescriptorSetLayout& descriptor_set_layout) {
        VkDescriptorSetAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = descriptor_pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &descriptor_set_layout;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set_));

        std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};
        
        // Binding 0 - object matrices uniform buffer
        writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[0].dstSet = descriptor_set_;
        writeDescriptorSets[0].dstBinding = 0;
        writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSets[0].pBufferInfo = &uniform_buffer_.descriptor;
        writeDescriptorSets[0].descriptorCount = 1;

        // Binding 1 - texture
        writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[1].dstSet = descriptor_set_;
        writeDescriptorSets[1].dstBinding = 1;
        writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSets[1].pImageInfo = texture_->getDescriptor();
        writeDescriptorSets[1].pBufferInfo = VK_NULL_HANDLE;
        writeDescriptorSets[1].descriptorCount = 1;
        
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }

    void prepareUniformBuffer(VulkanDevice* device, glm::mat4& proj, glm::mat4 view, glm::mat4 model) {
        uniform_buffer_.usage_flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        uniform_buffer_.property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uniform_buffer_.size = sizeof(Matrices);
        uniform_buffer_.device = *device;
        device->createBuffer(uniform_buffer_);
        VK_CHECK_RESULT(uniform_buffer_.map());

        updateUniformBuffer(proj, view, model);
    }

    void updateUniformBuffer(glm::mat4& proj, glm::mat4 view, glm::mat4 model) {
        matrices.projection_ = proj;
        matrices.view_ = view;
        matrices.model_ = model;

        std::memcpy(uniform_buffer_.mapped, &matrices, sizeof(matrices));

        uniform_buffer_.setupDescriptor();
    }
};


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
        device_ = std::make_unique<VulkanDevice>(instance_, surface_);
        swap_chain_ = VulkanSwapchain::createSwapChain(device_.get(), surface_, window_);
        createCommandBuffers();
        createSyncObjects();
        createScene();
        createRenderPass();
        setupDescriptors();
        createGraphicsPipeline();

        createFramebuffers();
    }

    void setupDescriptors() {
        createDescriptorSetLayout();
        createDescriptorPool();
        createSceneDescriptors();
    }

    void createSceneDescriptors() {
        for (auto& object : scene_objects_) {
            object.setupDescriptor(*device_, descriptor_pool_, descriptor_set_layout_);
        }
    }

    void createCommandBuffers() {
        draw_command_buffers.resize(swap_chain_->getImageCount());
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = device_->getCommandPool();
        alloc_info.commandBufferCount = draw_command_buffers.size();
        VK_CHECK_RESULT(vkAllocateCommandBuffers(*device_, &alloc_info, draw_command_buffers.data()));
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
        sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_layout_binding.descriptorCount = 1;
        sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        sampler_layout_binding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {ubo_layout_binding, sampler_layout_binding};

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = bindings.size();
        layout_info.pBindings = bindings.data();

        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*device_, &layout_info, nullptr, &descriptor_set_layout_));
    }

    void createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> pool_sizes{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = static_cast<uint32_t>(scene_objects_.size());
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(scene_objects_.size());

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = pool_sizes.size();
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<uint32_t>(scene_objects_.size());

        VK_CHECK_RESULT(vkCreateDescriptorPool(*device_, &pool_info, nullptr, &descriptor_pool_));
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK_RESULT(vkCreateSemaphore(*device_, &semaphore_info, nullptr, &image_available_semaphore_));
        VK_CHECK_RESULT(vkCreateSemaphore(*device_, &semaphore_info, nullptr, &render_finished_semaphore_));

        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        fences_.resize(draw_command_buffers.size());
        for (auto& fence: fences_) {
            VK_CHECK_RESULT(vkCreateFence(*device_, &fence_create_info, nullptr, &fence));
        }
    }

    void recordCommandBuffer(uint32_t image_index) {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = nullptr;

        VK_CHECK_RESULT(vkBeginCommandBuffer(draw_command_buffers[image_index], &begin_info));

        VkExtent2D extent = swap_chain_->getExtent();

        VkRenderPassBeginInfo render_pass_info;
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass_;
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent.width = extent.width;
        render_pass_info.renderArea.extent.height = extent.height;
        render_pass_info.pNext = VK_NULL_HANDLE;

        VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkClearValue clear_depth{};
        clear_depth.depthStencil = {1.0f, 0};
        std::array<VkClearValue, 2> clear_values = {clear_color, clear_depth};
        render_pass_info.clearValueCount = clear_values.size();
        render_pass_info.pClearValues = clear_values.data();

        render_pass_info.framebuffer = framebuffers_[image_index];

        vkCmdBeginRenderPass(draw_command_buffers[image_index], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(draw_command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(draw_command_buffers[image_index], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(draw_command_buffers[image_index], 0, 1, &scissor);
    
        for (auto& obj : scene_objects_) {
            auto current_time = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time_).count();

            glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            VkExtent2D extent = swap_chain_->getExtent();
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), extent.width / static_cast<float>(extent.height), 0.1f, 10.0f);
            proj[1][1] *= -1;

            obj.prepareUniformBuffer(device_.get(), proj, view, model);            

            vkCmdBindDescriptorSets(draw_command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, obj.getDescriptorSet(), 0, nullptr);
            obj.model_->draw(draw_command_buffers[image_index], pipeline_layout_);
        }

        vkCmdEndRenderPass(draw_command_buffers[image_index]);

        if (vkEndCommandBuffer(draw_command_buffers[image_index]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    void createFramebuffers() {
        framebuffers_.resize(swap_chain_->getImageCount());

        for (size_t i = 0; i < framebuffers_.size(); ++i) {
            std::vector<VkImageView> attachments = {
                swap_chain_->getImageView(i),
                swap_chain_->getDepthImageView()
            };

            VkExtent2D extent = swap_chain_->getExtent();

            VkFramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = render_pass_;
            framebuffer_info.attachmentCount = attachments.size();
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = extent.width;
            framebuffer_info.height = extent.height;
            framebuffer_info.layers = 1;

            VK_CHECK_RESULT(vkCreateFramebuffer(*device_, &framebuffer_info, nullptr, &framebuffers_[i]));
        }
    }
    
    void createRenderPass() {
        VkAttachmentDescription color_attachment{};
        color_attachment.format = swap_chain_->getImageFormat();
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
        depth_attachment.format = swap_chain_->getDepthFormat();
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

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = attachments.size();
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        VK_CHECK_RESULT(vkCreateRenderPass(*device_, &render_pass_info, nullptr, &render_pass_));
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

        VkExtent2D extent = swap_chain_->getExtent();

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

        VK_CHECK_RESULT(vkCreatePipelineLayout(*device_, &pipeline_layout_info, nullptr, &pipeline_layout_));

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

        VK_CHECK_RESULT(vkCreateGraphicsPipelines(*device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline_));

        vkDestroyShaderModule(*device_, frag_shader_module, nullptr);
        vkDestroyShaderModule(*device_, vert_shader_module, nullptr);
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shader_module;
        VK_CHECK_RESULT(vkCreateShaderModule(*device_, &create_info, nullptr, &shader_module));

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
        
        vkDeviceWaitIdle(*device_);

        createFramebuffers();
    }
   
    void mainLoop() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(*device_);
    }

    void drawFrame() {
        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(*device_, *swap_chain_, UINT64_MAX, image_available_semaphore_, VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }        
        
        recordCommandBuffer(image_index);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &image_available_semaphore_;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &render_finished_semaphore_;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &draw_command_buffers[image_index];

        if (vkQueueSubmit(device_->getGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_finished_semaphore_;

        VkSwapchainKHR swap_chains[] = {*swap_chain_};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swap_chains;
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr;

        result = vkQueuePresentKHR(device_->getPresentationQueue(), &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
            framebuffer_resized_ = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        vkDeviceWaitIdle(*device_);
    }

    void createScene() {
        start_time_ = std::chrono::high_resolution_clock::now();
        Sphere sphere;
        sphere.loadAssets(device_.get());
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        VkExtent2D extent = swap_chain_->getExtent();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), extent.width / static_cast<float>(extent.height), 0.1f, 10.0f);
        proj[1][1] *= -1;
        sphere.prepareUniformBuffer(device_.get(), proj, view, model);
        scene_objects_.push_back(std::move(sphere));
    }

    void cleanup() {
        vkDestroyPipeline(*device_, graphics_pipeline_, nullptr);
        vkDestroyPipelineLayout(*device_, pipeline_layout_, nullptr);
        vkDestroyRenderPass(*device_, render_pass_, nullptr);
        
        vkDestroyDevice(*device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);

        swap_chain_.reset();
        device_.reset();

        glfwDestroyWindow(window_);
        
        glfwTerminate();
    }

    VkDescriptorPool descriptor_pool_;
    VkDescriptorSetLayout descriptor_set_layout_;
    bool framebuffer_resized_ = false;
    VkPipeline graphics_pipeline_;
    VkInstance instance_;
    VkSemaphore image_available_semaphore_;
    VkPipelineLayout pipeline_layout_;
    VkSemaphore render_finished_semaphore_;
    VkRenderPass render_pass_;
    Runfiles* runfiles_;
    VkSurfaceKHR surface_;

    uint32_t current_buffer = 0;
    std::vector<VkCommandBuffer> draw_command_buffers;
    std::vector<VkFence> fences_;

    std::vector<VkFramebuffer> framebuffers_;
    VkBuffer vertex_buffer_;    
    VkDeviceMemory vertex_buffer_memory_;
    
    std::unique_ptr<VulkanDevice> device_;
    std::unique_ptr<VulkanSwapchain> swap_chain_;
    std::vector<Sphere> scene_objects_;

    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    
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