#include "file_utils.hpp"

#include <common/debug.hpp>

namespace FileUtils {
    std::vector<u8> read_file_bytes(const std::string& path) {
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
}