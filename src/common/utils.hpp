#ifndef GEMINO_UTILS_HPP
#define GEMINO_UTILS_HPP

#include <common/types.hpp>
#include <common/debug.hpp>
#include <vector>
#include <string>
#include <fstream>

namespace Utils {
    u32 calculate_mipmap_levels_x(u32 width);
    u32 calculate_mipmap_levels_xy(u32 width, u32 height);
    u32 calculate_mipmap_levels_xyz(u32 width, u32 height, u32 depth);

    std::vector<u8> read_file_bytes(const std::string& path);
}

#endif