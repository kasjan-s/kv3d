#pragma once

#include <array>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan_core.h>


struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex_coords;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();

    bool operator==(const Vertex& other) const;
};

namespace std {
    template<> struct hash<Vertex> {
        // Copilot hash generated hash function.
        size_t operator()(Vertex const& vertex) const {
            size_t hash_pos = hash<glm::vec3>()(vertex.pos);
            size_t hash_color = hash<glm::vec3>()(vertex.color);
            size_t hash_tex_coords = hash<glm::vec2>()(vertex.tex_coords);
            size_t hash_normal = hash<glm::vec3>()(vertex.normal);

            // Combine using a common bit-mixing pattern
            size_t hash_result = hash_pos;
            hash_result ^= hash_color + 0x9e3779b9 + (hash_result << 6) + (hash_result >> 2);
            hash_result ^= hash_tex_coords + 0x9e3779b9 + (hash_result << 6) + (hash_result >> 2);
            hash_result ^= hash_normal + 0x9e3779b9 + (hash_result << 6) + (hash_result >> 2);

            return hash_result;
        }
    };
}