#ifndef GEMINO_VERTEX_HPP
#define GEMINO_VERTEX_HPP

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <common/types.hpp>
#include <vector>

struct Vertex {
    bool operator==(const Vertex& other) const {
        return pos == other.pos && texcoord == other.texcoord && normal == other.normal;
    }

    glm::vec3 pos{};
    glm::vec3 normal{};
    glm::vec2 texcoord{};

    static std::vector<VkVertexInputBindingDescription> get_binding_description() {
        return { VkVertexInputBindingDescription{
            .binding = 0U,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }};
    }

    static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() {
        return {
            VkVertexInputAttributeDescription{
                .location = 0U,
                .binding = 0U,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<u32>(offsetof(Vertex, pos)),
            },
            VkVertexInputAttributeDescription{
                .location = 1U,
                .binding = 0U,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = static_cast<u32>(offsetof(Vertex, normal)),
            },
            VkVertexInputAttributeDescription{
                .location = 2U,
                .binding = 0U,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = static_cast<u32>(offsetof(Vertex, texcoord)),
            }
        };
    }
};

#endif