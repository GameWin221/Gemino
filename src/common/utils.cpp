#include "utils.hpp"

u32 Utils::calculate_mipmap_levels_x(u32 width) {
    return static_cast<u32>(std::floor(std::log2(width))) + 1U;
}
u32 Utils::calculate_mipmap_levels_xy(u32 width, u32 height) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1U;
}
u32 Utils::calculate_mipmap_levels_xyz(u32 width, u32 height, u32 depth) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(std::max(width, height), depth)))) + 1U;
}

std::vector<u8> Utils::read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        DEBUG_ERROR("Failed to read bytes from file: \"" << path << "\"")
        return std::vector<u8>{};
    }

    usize file_size = static_cast<usize>(file.tellg());
    std::vector<u8> buffer(file_size);

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    file.close();

    return buffer;
}