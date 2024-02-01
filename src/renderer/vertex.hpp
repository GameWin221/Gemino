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

template<>
struct std::hash<Vertex> {
    std::size_t operator()(const Vertex& v) const noexcept {
        std::size_t p = (((std::size_t)v.pos.x) >> 2) | (((std::size_t)v.pos.y) << 14) | (((std::size_t)v.pos.z) << 34);
        std::size_t n = (((std::size_t)v.normal.x) >> 4) | (((std::size_t)v.normal.y) << 22) | (((std::size_t)v.normal.z) << 15);
        std::size_t t = (((std::size_t)v.texcoord.x) >> 1) | (((std::size_t)v.texcoord.y) << 16);

        return p ^ (n << 1) ^ (t << 2);
    }
};

#endif