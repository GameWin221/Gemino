#include "utils.hpp"

u32 Utils::nearest_pot_floor(u32 x) {
    return (1U << static_cast<u32>(std::floor(std::log2(x))));
}

u32 Utils::calculate_mipmap_levels_x(u32 width) {
    return static_cast<u32>(std::floor(std::log2(width))) + 1U;
}
u32 Utils::calculate_mipmap_levels_xy(u32 width, u32 height) {
    return static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1U;
}
u32 Utils::calculate_mipmap_levels_xyz(u32 width, u32 height, u32 depth) {
    return static_cast<u32>(std::floor(std::log2(std::max(std::max(width, height), depth)))) + 1U;
}

std::vector<u8> Utils::read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        DEBUG_ERROR("Failed to read bytes from file: \"" << path << "\"")
        return std::vector<u8>{};
    }

    auto file_size = file.tellg();
    std::vector<u8> buffer(static_cast<usize>(file_size));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    file.close();

    return buffer;
}

std::vector<std::string> Utils::read_file_lines(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        DEBUG_ERROR("Failed to read lines from file: \"" << path << "\"")
        return std::vector<std::string>{};
    }

    std::vector<std::string> buffer{};

    std::string line;
    while(std::getline(file, line)) {
        buffer.push_back(line);
    }

    return buffer;
}

std::string Utils::get_file_name(const std::string &path, bool with_extension) {
    std::string path_no_ext{};
    if (with_extension && path.find('.') != std::string::npos) {
        path_no_ext = path.substr(0, path.find_last_of('.'));
    } else {
        path_no_ext = path;
    }

    if (path_no_ext.find('/') != std::string::npos) {
        return path_no_ext.substr(path_no_ext.find_last_of('/') + 1);
    } else if (path_no_ext.find('\\') != std::string::npos) {
        return path_no_ext.substr(path_no_ext.find_last_of('\\') + 1);
    } else {
        return path_no_ext;
    }
}
std::string Utils::get_directory(const std::string &path) {
    if (path.find('/') != std::string::npos) {
        return path.substr(0, path.find_last_of('/') + 1);
    } else if (path.find('\\') != std::string::npos) {
        return path.substr(0, path.find_last_of('\\') + 1);
    } else {
        return "/";
    }
}