#ifndef GEMINO_VERTEX_HPP
#define GEMINO_VERTEX_HPP

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <common/types.hpp>
#include <vector>

struct Vertex {
    bool operator==(const Vertex &other) const {
        return pos == other.pos && texcoord == other.texcoord && normal == other.normal;
    }

    glm::f32vec3 pos{};
    glm::i8vec4 normal{};
    glm::u16vec2 texcoord{};
};

namespace std {
    template<>
    struct hash<Vertex> {
        std::size_t operator()(const Vertex &v) const noexcept {
            std::size_t p = (((std::size_t)v.pos.x) >> 2) | (((std::size_t)v.pos.y) << 14) | (((std::size_t)v.pos.z) << 34);
            std::size_t n = (((std::size_t)v.normal.x) >> 4) | (((std::size_t)v.normal.y) << 22) | (((std::size_t)v.normal.z) << 15);
            std::size_t t = (((std::size_t)v.texcoord.x) >> 1) | (((std::size_t)v.texcoord.y) << 16);

            return p ^ (n << 1) ^ (t << 2);
        }
    };
}


#endif