#ifndef GEMINO_UTILS_HPP
#define GEMINO_UTILS_HPP

#include <common/types.hpp>
#include <common/debug.hpp>

#include <render_api/vertex.hpp>

#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace Utils {
    struct SubMeshImportData {
        std::vector<Vertex> vertices{};
        std::vector<u32> indices{};

        glm::vec3 center_offset{};
        f32 radius{};

        void free() {
            vertices = std::vector<Vertex>();
            indices = std::vector<u32>();
        }
    };
    struct MeshImportData {
        std::vector<SubMeshImportData> sub_meshes{};

        void free() {
            for(auto& sub_mesh : sub_meshes) {
                sub_mesh.free();
            }

            sub_meshes = std::vector<SubMeshImportData>();
        }
    };

    template<typename T>
    struct ImageImportData {
        T* pixels{};
        u32 width{};
        u32 height{};
        u32 bytes_per_pixel{};

        void free() {
            std::free(pixels);
            pixels = nullptr;
        }
    };

    u32 nearest_pot_floor(u32 x);

    u32 calculate_mipmap_levels_x(u32 width);
    u32 calculate_mipmap_levels_xy(u32 width, u32 height);
    u32 calculate_mipmap_levels_xyz(u32 width, u32 height, u32 depth);

    std::vector<u8> read_file_bytes(const std::string& path);
    std::vector<std::string> read_file_lines(const std::string& path);

    MeshImportData load_gltf(const std::string& path);
    ImageImportData<u8> load_u8_image(const std::string& path, u32 desired_channels);

    constexpr usize align(usize alignment, usize size) {
        return ((size - 1) / alignment + 1) * alignment;
    }
    constexpr usize div_ceil(usize a, usize b) {
        return (a - 1) / b + 1;
    }
}

#endif