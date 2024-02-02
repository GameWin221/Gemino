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

Utils::MeshImportData Utils::load_obj(const std::string& path) {
    MeshImportData data{};

    auto lines = read_file_lines(path);
    if(lines.empty()) {
        DEBUG_PANIC("Failed to read obj data from \"" << path << "\"")
    }

    std::vector<glm::vec3> positions{};
    std::vector<glm::vec3> normals{};
    std::vector<glm::vec2> texcoords{};

    std::unordered_map<Vertex, uint32_t> vert_map{};

    for(const auto& line : lines) {
        switch (line[0]) {
            case 'o': {
                data.sub_meshes.push_back(SubMeshImportData{});
            } break;
            case 'v':
                switch (line[1]) {
                    case ' ': {
                        glm::vec3 pos;
                        if (!sscanf(line.c_str(), "v %f %f %f", &pos.x, &pos.y, &pos.z)) {
                            DEBUG_PANIC("Invalid position data!")
                        }

                        positions.push_back(pos);
                    }   break;
                    case 't': {
                        glm::vec2 uv;
                        if (!sscanf(line.c_str(), "vt %f %f", &uv.x, &uv.y)) {
                            DEBUG_PANIC("Invalid uv data!")
                        }

                        texcoords.push_back(uv);
                    }   break;
                    case 'n': {
                        glm::vec3 normal;
                        if (!sscanf(line.c_str(), "vn %f %f %f", &normal.x, &normal.y, &normal.z)) {
                            DEBUG_PANIC("Invalid normal data!")
                        }

                        normals.push_back(normal);
                    }   break;
                    default:
                        break;
                }
                break;
            case 'f': {
                u32 pos[3];
                u32 uv[3];
                u32 normal[3];

                if (!sscanf(line.c_str(), "f %d/%d/%d %d/%d/%d %d/%d/%d", &pos[0], &uv[0], &normal[0], &pos[1], &uv[1], &normal[1], &pos[2], &uv[2], &normal[2])) {
                    DEBUG_PANIC("Invalid triangle data!")
                }

                Vertex tri[3]{
                    Vertex{ positions[pos[0] - 1], normals[normal[0] - 1], texcoords[uv[0] - 1] },
                    Vertex{ positions[pos[1] - 1], normals[normal[1] - 1], texcoords[uv[1] - 1] },
                    Vertex{ positions[pos[2] - 1], normals[normal[2] - 1], texcoords[uv[2] - 1] }
                };

                std::vector<u32>& indices = data.sub_meshes.back().indices;
                std::vector<Vertex>& vertices = data.sub_meshes.back().vertices;

                for (const auto& v : tri) {
                    if (vert_map.contains(v)) {
                        indices.push_back(vert_map.at(v));
                    } else {
                        u32 index = static_cast<u32>(vertices.size());

                        vert_map[v] = index;
                        indices.push_back(index);
                        vertices.emplace_back(v);
                    }
                }
            }   break;
            default:
                break;
        }
    }

    DEBUG_LOG("Loaded meshes from \"" << path << "\"")

    return data;
}

Utils::ImageImportData<u8> Utils::load_u8_image(const std::string& path, u32 desired_channels) {
    ImageImportData<u8> data{};

    stbi_set_flip_vertically_on_load(true);

    i32 width, height, channels;
    data.pixels = stbi_load(path.c_str(), &width, &height, &channels, static_cast<i32>(desired_channels));
    if(!data.pixels) {
        DEBUG_PANIC("Failed to load image from \"" << path << "\"")
    }

    data.width = static_cast<u32>(width);
    data.height = static_cast<u32>(height);
    data.bytes_per_pixel = desired_channels;

    DEBUG_LOG("Loaded image from \"" << path << "\"")

    return data;
}
