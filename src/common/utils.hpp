#ifndef GEMINO_UTILS_HPP
#define GEMINO_UTILS_HPP

#include <common/types.hpp>
#include <common/debug.hpp>

#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace Utils {
    u32 nearest_pot_floor(u32 x);

    u32 calculate_mipmap_levels_x(u32 width);
    u32 calculate_mipmap_levels_xy(u32 width, u32 height);
    u32 calculate_mipmap_levels_xyz(u32 width, u32 height, u32 depth);

    std::vector<u8> read_file_bytes(const std::string& path);
    std::vector<std::string> read_file_lines(const std::string& path);

    std::string get_file_name(const std::string &path, bool with_extension = true);
    std::string get_directory(const std::string &path);

    constexpr usize align(usize alignment, usize size) {
        return ((size - 1) / alignment + 1) * alignment;
    }
    constexpr usize div_ceil(usize a, usize b) {
        return (a - 1) / b + 1;
    }
}

#endif