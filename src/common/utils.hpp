#ifndef GEMINO_UTILS_HPP
#define GEMINO_UTILS_HPP

#include <common/types.hpp>
#include <common/debug.hpp>

#include <renderer/vertex.hpp>

#include <stb/stb_image.h>

#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace Utils {
    struct SubMeshImportData {
        std::vector<Vertex> vertices{};
        std::vector<u32> indices{};
    };
    struct MeshImportData {
        std::vector<SubMeshImportData> sub_meshes{};

        void free() {
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
            stbi_image_free(pixels);
            pixels = nullptr;
        }
    };

    u32 calculate_mipmap_levels_x(u32 width);
    u32 calculate_mipmap_levels_xy(u32 width, u32 height);
    u32 calculate_mipmap_levels_xyz(u32 width, u32 height, u32 depth);

    std::vector<u8> read_file_bytes(const std::string& path);
    std::vector<std::string> read_file_lines(const std::string& path);

    MeshImportData load_obj(const std::string& path);
    ImageImportData<u8> load_u8_image(const std::string& path, u32 desired_channels);

    constexpr usize align(usize alignment, usize size) {
        return (((size - 1) / alignment) + 1) * alignment;
    }
}

#endif