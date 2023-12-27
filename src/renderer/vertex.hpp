#ifndef GEMINO_VERTEX_HPP
#define GEMINO_VERTEX_HPP

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>

struct Vertex {
    Vertex& operator=(const Vertex& other) {
        pos = other.pos;
        normal = other.normal;

        texcoord = other.texcoord;
        return *this;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && texcoord == other.texcoord && normal == other.normal;
    }

    glm::vec3 pos{};
    glm::vec3 normal{};
    glm::vec2 texcoord{};

    static const std::vector<VkVertexInputBindingDescription> get_binding_description() {
        return { VkVertexInputBindingDescription{
            .binding = 0U,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }};
    }

    static const std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() {
        return {
            VkVertexInputAttributeDescription{
                .location = 0U,
                .binding = 0U,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, pos),
            },
            VkVertexInputAttributeDescription{
                .location = 1U,
                .binding = 0U,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, normal),
            },
            VkVertexInputAttributeDescription{
                .location = 2U,
                .binding = 0U,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, texcoord),
            }
        };
    }
};

#endif