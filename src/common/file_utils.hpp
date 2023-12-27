#ifndef GEMINO_FILE_UTILS_HPP
#define GEMINO_FILE_UTILS_HPP

#include <vector>
#include <fstream>
#include <common/types.hpp>

namespace FileUtils {
    std::vector<u8> read_file_bytes(const std::string& path);
};


#endif
