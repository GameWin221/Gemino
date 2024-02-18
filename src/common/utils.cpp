#include "utils.hpp"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

u32 Utils::nearest_pot_floor(u32 x) {
    return (1U << static_cast<uint32_t>(std::floor(std::log2(x))));
}

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

static void post_process_sub_mesh(Utils::SubMeshImportData& sub_mesh) {
    glm::vec3 pos_min(std::numeric_limits<f32>::max());
    glm::vec3 pos_max(std::numeric_limits<f32>::min());

    for(const auto& vertex : sub_mesh.vertices) {
        if(vertex.pos.x < pos_min.x) {
            pos_min.x = vertex.pos.x;
        }
        if(vertex.pos.y < pos_min.y) {
            pos_min.y = vertex.pos.y;
        }
        if(vertex.pos.z < pos_min.z) {
            pos_min.z = vertex.pos.z;
        }

        if(vertex.pos.x > pos_max.x) {
            pos_max.x = vertex.pos.x;
        }
        if(vertex.pos.y > pos_max.y) {
            pos_max.y = vertex.pos.y;
        }
        if(vertex.pos.z > pos_max.z) {
            pos_max.z = vertex.pos.z;
        }
    }

    sub_mesh.center = (pos_min + pos_max) / 2.0f;

    f32 max_dist{};
    for(const auto& vertex : sub_mesh.vertices) {
        f32 dist = glm::distance(sub_mesh.center, vertex.pos);

        if(dist > max_dist) {
            max_dist = dist;
        }
    }

    sub_mesh.radius = max_dist;
}
static void post_process_mesh(Utils::MeshImportData& mesh) {
    for(auto& sub_mesh : mesh.sub_meshes) {
        post_process_sub_mesh(sub_mesh);
    }
}

static void process_gltf_mesh(Utils::MeshImportData& data, const tinygltf::Model& model, const tinygltf::Mesh& mesh) {
    for(const auto& primitive : mesh.primitives) {
        data.sub_meshes.push_back(Utils::SubMeshImportData{});
        Utils::SubMeshImportData& sub_mesh_data = data.sub_meshes.back();

        std::vector<glm::vec3> positions{};
        std::vector<glm::vec3> normals{};
        std::vector<glm::vec2> texcoords{};

        for (const auto& [attrib_name, attrib_id] : primitive.attributes) {
            const tinygltf::Accessor& accessor = model.accessors[attrib_id];
            const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

            usize element_count = accessor.count;
            if (attrib_name == "POSITION") {
                positions.resize(accessor.count);

                DEBUG_ASSERT(element_count * sizeof(float) * 3 == buffer_view.byteLength)
                std::memcpy(positions.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(buffer.data.data()) + buffer_view.byteOffset), buffer_view.byteLength);
            } else if (attrib_name == "NORMAL") {
                normals.resize(element_count);

                DEBUG_ASSERT(element_count * sizeof(float) * 3 == buffer_view.byteLength)
                std::memcpy(normals.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(buffer.data.data()) + buffer_view.byteOffset), buffer_view.byteLength);
            } else if (attrib_name == "TEXCOORD_0") {
                if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    texcoords.resize(element_count);

                    DEBUG_ASSERT(element_count * sizeof(float) * 2 == buffer_view.byteLength)
                    std::memcpy(texcoords.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(buffer.data.data()) + buffer_view.byteOffset), buffer_view.byteLength);
                } else {
                    DEBUG_PANIC("Unsupported gltf TEXCOORD_0 attribute component type for mesh \"" << mesh.name << "\"")
                }
            }
        }

        if(positions.empty()) {
            DEBUG_PANIC("Failed to get position data from mesh \"" << mesh.name << "\"")
        }
        if(normals.empty()) {
            DEBUG_PANIC("Failed to get normal data from mesh \"" << mesh.name << "\"")
        }
        if(texcoords.empty()) {
            DEBUG_PANIC("Failed to get texcoord data from mesh \"" << mesh.name << "\"")
        }

        DEBUG_ASSERT(positions.size() == normals.size() && normals.size() == texcoords.size())

        data.sub_meshes.back().vertices.resize(positions.size());
        for(usize i{}; i < positions.size(); ++i) {
            sub_mesh_data.vertices[i].pos = positions[i];
            sub_mesh_data.vertices[i].normal = normals[i];
            sub_mesh_data.vertices[i].texcoord = texcoords[i];
        }

        const tinygltf::Accessor& index_accessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& index_buffer_view = model.bufferViews[index_accessor.bufferView];
        const tinygltf::Buffer& index_buffer = model.buffers[index_buffer_view.buffer];
        usize index_count = index_accessor.count;
        i32 index_type = index_accessor.componentType;

        sub_mesh_data.indices.resize(index_count);

        if(index_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            DEBUG_ASSERT(index_count * sizeof(u8) == index_buffer_view.byteLength)
            for(usize offset = index_buffer_view.byteOffset, i{}; i < index_count; offset += sizeof(u8), ++i) {
                sub_mesh_data.indices[i] = static_cast<u32>(*reinterpret_cast<u8*>(reinterpret_cast<usize>(index_buffer.data.data()) + offset));
            }
        } else if(index_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            DEBUG_ASSERT(index_count * sizeof(u16) == index_buffer_view.byteLength)
            for(usize offset = index_buffer_view.byteOffset, i{}; i < index_count; offset += sizeof(u16), ++i) {
                sub_mesh_data.indices[i] = static_cast<u32>(*reinterpret_cast<u16*>(reinterpret_cast<usize>(index_buffer.data.data()) + offset));
            }
        } else if(index_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            DEBUG_ASSERT(index_count * sizeof(u32) == index_buffer_view.byteLength)
            std::memcpy(sub_mesh_data.indices.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(index_buffer.data.data()) + index_buffer_view.byteOffset), index_buffer_view.byteLength);
        } else {
            DEBUG_PANIC("Unsupported gltf indices component type for mesh \"" << mesh.name << "\"")
        }
    }
}
static void process_gltf_node(Utils::MeshImportData& data, const tinygltf::Model& model, const tinygltf::Node& node) {
    if(node.mesh >= 0) {
        process_gltf_mesh(data, model, model.meshes[node.mesh]);
    }

    for(const auto& child_id : node.children) {
        process_gltf_node(data, model, model.nodes[child_id]);
    }
}

Utils::MeshImportData Utils::load_gltf(const std::string& path) {
    MeshImportData data{};

    tinygltf::TinyGLTF loader{};
    tinygltf::Model model{};
    std::string err{}, warn{};

    if (!loader.LoadASCIIFromFile(&model, &err, &warn, path)) {
        DEBUG_PANIC("Failed to read gltf data from \"" << path << "\"")
    }

    if (!warn.empty()) {
        DEBUG_WARNING("GLTF Import warning from \"" << path << " \": " << warn)
    }

    if (!err.empty()) {
        DEBUG_PANIC("GLTF Import error from \"" << path << " \" warning: " << warn)
    }

    DEBUG_LOG("Loading meshes from \"" << path << "\"")

    for(const auto& node : model.nodes) {
        process_gltf_node(data, model, node);
    }

    post_process_mesh(data);

    DEBUG_LOG("Loaded meshes from \"" << path << "\"")

    return data;
}

Utils::ImageImportData<u8> Utils::load_u8_image(const std::string& path, u32 desired_channels) {
    ImageImportData<u8> data{};

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
