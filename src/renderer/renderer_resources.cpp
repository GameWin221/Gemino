#include "renderer.hpp"

#include <stb/stb_image.h>

#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>

static glm::vec3 calculate_center_offset(const std::vector<Vertex> &vertices) {
    glm::vec3 center_offset{};
    for(const auto &v : vertices) {
        center_offset += v.pos;
    }

    center_offset /= (f32)vertices.size();

    return center_offset;
}
static f32 calculate_radius(const std::vector<Vertex> &vertices, const glm::vec3 &center_offset) {
    f32 max_dist{};
    for(const auto& vertex : vertices) {
        f32 dist = glm::distance(center_offset, vertex.pos);

        if(dist > max_dist) {
            max_dist = dist;
        }
    }

    return max_dist;
}

static void process_gltf_node(SceneCreateInfo &scene, const tinygltf::Model &model, u32 node_id) {
    const tinygltf::Node &node = model.nodes[node_id];

    ObjectCreateInfo object{};

    if (!node.translation.empty()) {
        object.local_position = glm::vec3(
            static_cast<f32>(node.translation[0]),
            static_cast<f32>(node.translation[1]),
            static_cast<f32>(node.translation[2])
        );
    }

    if (!node.rotation.empty()) {
        object.local_rotation = glm::quat(
            static_cast<f32>(node.rotation[3]),
            static_cast<f32>(node.rotation[0]),
            static_cast<f32>(node.rotation[1]),
            static_cast<f32>(node.rotation[2])
        );
    }

    if (!node.scale.empty()) {
        object.local_scale = glm::vec3(
            static_cast<f32>(node.scale[0]),
            static_cast<f32>(node.scale[1]),
            static_cast<f32>(node.scale[2])
        );
    }

    u32 object_id = static_cast<u32>(scene.objects.size());
    scene.objects.push_back(object);
    scene.children.emplace_back();

    if (node.mesh != -1) {
        for (const auto &primitive : scene.meshes_primitives[node.mesh]) {
            u32 child_object_id = static_cast<u32>(scene.objects.size());

            ObjectCreateInfo child_info{
                .mesh = scene.meshes[primitive],
                .material = scene.meshes_default_materials[primitive],
            };

            scene.objects.push_back(child_info);
            scene.children.emplace_back();
            scene.children[object_id].push_back(child_object_id);
        }
    }

    for(const auto &child_node_id : node.children) {
        u32 child_object_id = static_cast<u32>(scene.objects.size());

        scene.children[object_id].push_back(child_object_id);

        process_gltf_node(scene, model, child_node_id);
    }
}

SceneCreateInfo Renderer::load_gltf_scene(const SceneLoadInfo &load_info) {
    SceneCreateInfo scene{};

    tinygltf::TinyGLTF loader{};
    tinygltf::Model model{};
    std::string err{}, warn{};

    if (!loader.LoadASCIIFromFile(&model, &err, &warn, load_info.path)) {

    }

    if (!warn.empty()) {
        DEBUG_WARNING("GLTF Import warning from \"" << load_info.path << "\": " << warn)
    }

    if (!err.empty()) {
        DEBUG_PANIC("GLTF Import error from \"" << load_info.path << "\": " << err)
    }

    DEBUG_LOG("Loading GLTF scene from \"" << load_info.path << "\"")

    if (model.defaultScene < 0) {
        DEBUG_ERROR("Failed to load a GLTF scene from \"" << load_info.path << "\" because it had no default scene specified.")
    }

    tinygltf::Scene &gltf_scene = model.scenes[model.defaultScene];
    //scene.name = gltf_scene.name;

    if (load_info.import_textures) {
        std::vector<bool> is_texture_srgb(model.textures.size(), false);
        for (const auto &material : model.materials) {
            i32 albedo_texture_id = material.pbrMetallicRoughness.baseColorTexture.index;

            // Only albedo should be sRGB
            if (albedo_texture_id != -1) {
                is_texture_srgb[albedo_texture_id] = true;
            }
        }

        u32 tex_id{};
        for (const auto &texture : model.textures) {
            // TODO: Handle samplers

            const auto &image = model.images[texture.source];

            if (!image.uri.empty()) {
                std::string directory = Utils::get_directory(load_info.path);

                scene.textures.push_back(load_u8_texture(TextureLoadInfo{
                    .path = directory + image.uri,
                    .is_srgb = is_texture_srgb[tex_id++]
                }));
            } else {
                DEBUG_PANIC("Failed to import image! No source URI provided!")
            }
        }
    }

    if (load_info.import_materials) {
        for (const auto &material : model.materials) {
            i32 albedo_texture_id = material.pbrMetallicRoughness.baseColorTexture.index;
            i32 roughness_texture_id = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            i32 metalness_texture_id = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            i32 normal_texture_id = material.normalTexture.index;

            //.name = material.name;
            scene.materials.push_back(create_material(MaterialCreateInfo {
                .albedo_texture = ((albedo_texture_id != -1 && load_info.import_textures) ? scene.textures[albedo_texture_id] : INVALID_HANDLE),
                .roughness_texture = ((roughness_texture_id != -1 && load_info.import_textures) ? scene.textures[roughness_texture_id] : INVALID_HANDLE),
                .metalness_texture = ((metalness_texture_id != -1 && load_info.import_textures) ? scene.textures[metalness_texture_id] : INVALID_HANDLE),
                .normal_texture = ((normal_texture_id != -1 && load_info.import_textures) ? scene.textures[normal_texture_id] : INVALID_HANDLE),
                //.metalness_factor = static_cast<f32>(material.pbrMetallicRoughness.metallicFactor),
                //.roughness_factor = static_cast<f32>(material.pbrMetallicRoughness.roughnessFactor),
                .color = glm::vec3(
                    material.pbrMetallicRoughness.baseColorFactor[0],
                    material.pbrMetallicRoughness.baseColorFactor[1],
                    material.pbrMetallicRoughness.baseColorFactor[2]
                )
            }));

            DEBUG_LOG("Loaded material \"" << material.name << "\" from \"" << load_info.path << "\"")
        }
    } else {
        scene.materials.resize(model.materials.size(), m_default_material);
    }

    scene.meshes_primitives.resize(model.meshes.size(), std::vector<u32>{});

    for (uint32_t i{}; i < static_cast<u32>(model.meshes.size()); ++i) {
        const auto &mesh = model.meshes[i];

        for(const auto &primitive : mesh.primitives) {
            //.name = mesh.name + std::string("_") + std::to_string(primitive_id++);

            DEBUG_ASSERT(primitive.mode == TINYGLTF_MODE_TRIANGLES);

            std::vector<glm::vec3> positions{};
            std::vector<glm::vec3> normals{};
            std::vector<glm::vec2> texcoords{};

            for (const auto &[attrib_name, attrib_id] : primitive.attributes) {
                const tinygltf::Accessor &accessor = model.accessors[attrib_id];
                const tinygltf::BufferView &buffer_view = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];

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

            std::vector<Vertex> vertices(positions.size());

            for(usize i{}; i < positions.size(); ++i) {
                vertices[i].pos = positions[i];
                vertices[i].normal = normals[i];
                vertices[i].texcoord = texcoords[i];
            }

            glm::vec3 center_offset = calculate_center_offset(vertices);
            f32 radius = calculate_radius(vertices, center_offset);

            const tinygltf::Accessor &index_accessor = model.accessors[primitive.indices];
            const tinygltf::BufferView &index_buffer_view = model.bufferViews[index_accessor.bufferView];
            const tinygltf::Buffer &index_buffer = model.buffers[index_buffer_view.buffer];
            u32 index_count = static_cast<u32>(index_accessor.count);
            i32 index_type = index_accessor.componentType;

            std::vector<u32> indices(index_count);

            if(index_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                DEBUG_ASSERT(index_count * sizeof(u8) == index_buffer_view.byteLength)
                for(usize offset = index_buffer_view.byteOffset, i{}; i < index_count; offset += sizeof(u8), ++i) {
                    indices[i] = static_cast<u32>(*reinterpret_cast<u8*>(reinterpret_cast<usize>(index_buffer.data.data()) + offset));
                }
            } else if(index_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                DEBUG_ASSERT(index_count * sizeof(u16) == index_buffer_view.byteLength)
                for(usize offset = index_buffer_view.byteOffset, i{}; i < index_count; offset += sizeof(u16), ++i) {
                    indices[i] = static_cast<u32>(*reinterpret_cast<u16*>(reinterpret_cast<usize>(index_buffer.data.data()) + offset));
                }
            } else if(index_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                DEBUG_ASSERT(index_count * sizeof(u32) == index_buffer_view.byteLength)
                std::memcpy(indices.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(index_buffer.data.data()) + index_buffer_view.byteOffset), index_buffer_view.byteLength);
            } else {
                DEBUG_PANIC("Unsupported gltf indices component type for mesh \"" << mesh.name << "\"")
            }

            auto primitive_mesh_handle = create_mesh(MeshCreateInfo {
                .lods {
                    MeshLODCreateInfo {
                        .vertex_data = vertices.data(),
                        .vertex_count = static_cast<u32>(vertices.size()),
                        .index_data = indices.data(),
                        .index_count = static_cast<u32>(indices.size()),
                        .center_offset = center_offset,
                        .radius = radius,
                    }
                },
                .cull_distance = 4000.0f
            });

            u32 mesh_id = static_cast<u32>(scene.meshes.size());
            scene.meshes.push_back(primitive_mesh_handle);
            scene.meshes_default_materials.push_back((primitive.material != -1 && load_info.import_materials) ? scene.materials[primitive.material] : m_default_material);

            scene.meshes_primitives[i].push_back(mesh_id);
        }

        DEBUG_LOG("Loaded mesh \"" << mesh.name << "\" from \"" << load_info.path << "\"")
    }

    for(const auto &node_id : gltf_scene.nodes) {
        u32 object_id = static_cast<u32>(scene.objects.size());

        scene.root_objects.push_back(static_cast<u32>(object_id));

        process_gltf_node(scene, model, node_id);
    }

    DEBUG_LOG("Loaded GLTF scene from \"" << load_info.path << "\"")

    return scene;
}

Handle<Mesh> Renderer::create_mesh(const MeshCreateInfo &create_info) {
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Mesh mesh {
        .center_offset = glm::vec3{},
        .radius = 0.0f,
        .cull_distance = create_info.cull_distance,
        .lod_bias = create_info.lod_bias,
        .lod_count = static_cast<u32>(create_info.lods.size())
    };

    if(mesh.lod_count == 0U) {
        DEBUG_PANIC("Mesh must use more than 0 LODs!")
    }
    if(mesh.lod_count > 8U) {
        DEBUG_PANIC("Mesh cannot use more than 8 LODs! lod_count = " << mesh.lod_count)
    }

    for(u32 lod_id{}; lod_id < mesh.lod_count; ++lod_id) {
        const auto &lod_info = create_info.lods[lod_id];

        mesh.lods[lod_id] = MeshLOD {
            .index_count = lod_info.index_count,
            .first_index = m_allocated_indices,
            .vertex_offset = static_cast<i32>(m_allocated_vertices)
        };

        if (static_cast<VkDeviceSize>(m_allocated_vertices) + static_cast<VkDeviceSize>(lod_info.vertex_count) > MAX_SCENE_VERTICES) {
            DEBUG_PANIC("Failed to allocate vertices! Allocating another " << lod_info.vertex_count << " vertices would result in a buffer overflow!")
        }
        if (static_cast<VkDeviceSize>(m_allocated_indices) + static_cast<VkDeviceSize>(lod_info.index_count) > MAX_SCENE_INDICES) {
            DEBUG_PANIC("Failed to allocate indices! Allocating another " << lod_info.vertex_count << " indices would result in a buffer overflow!")
        }

        // Pick the biggest bounding sphere out of all LODs
        if (lod_info.radius > mesh.radius) {
            mesh.radius = lod_info.radius;
            mesh.center_offset = lod_info.center_offset;
        }

        Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
            .size = (lod_info.vertex_count * sizeof(Vertex)) + (lod_info.index_count * sizeof(u32)) + sizeof(MeshLOD),
            .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
        });

        m_api.record_and_submit_once([this, &lod_info, staging_buffer](Handle<CommandList> cmd) {
            m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, lod_info.vertex_data, lod_info.vertex_count * sizeof(Vertex), 0);
            m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_vertex_buffer, { VkBufferCopy{
                .dstOffset = m_allocated_vertices * sizeof(Vertex),
                .size = lod_info.vertex_count * sizeof(Vertex)
            }});

            m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, lod_info.index_data, lod_info.index_count * sizeof(u32), lod_info.vertex_count * sizeof(Vertex));
            m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_index_buffer, { VkBufferCopy{
                .srcOffset = lod_info.vertex_count * sizeof(Vertex),
                .dstOffset = m_allocated_indices * sizeof(u32),
                .size = lod_info.index_count * sizeof(u32)
            }});
        });

        m_api.m_resource_manager->destroy_buffer(staging_buffer);

        m_allocated_vertices += lod_info.vertex_count;
        m_allocated_indices += lod_info.index_count;
    }

    Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Mesh),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, &mesh, sizeof(Mesh));

    Handle<Mesh> mesh_handle = m_mesh_allocator.alloc(mesh);

    m_api.record_and_submit_once([this, mesh_handle, staging_buffer](Handle<CommandList> cmd) {
        m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_mesh_buffer, {VkBufferCopy{
            .dstOffset = mesh_handle * sizeof(Mesh),
            .size = sizeof(Mesh)
        }});
    });

    m_api.m_resource_manager->destroy_buffer(staging_buffer);

    return mesh_handle;
}
void Renderer::destroy_mesh(Handle<Mesh> mesh_handle) {
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    if(!m_mesh_allocator.is_handle_valid(mesh_handle)) {
        DEBUG_PANIC("Cannot delete mesh - Mesh with a handle id: = " << mesh_handle << ", does not exist!")
    }

    const Mesh &mesh = m_mesh_allocator.get_element(mesh_handle);

    m_mesh_allocator.free(mesh_handle);
}

Handle<Texture> Renderer::load_u8_texture(const TextureLoadInfo &load_info) {
    i32 width, height, channels;
    u8 *pixels = stbi_load(load_info.path.c_str(), &width, &height, &channels, static_cast<i32>(load_info.desired_channels));
    if(!pixels) {
        DEBUG_PANIC("Failed to load image from \"" << load_info.path << "\"")
    }

    DEBUG_LOG("Loaded image from \"" << load_info.path << "\"")

    auto handle = create_u8_texture(TextureCreateInfo {
        .pixel_data = pixels,
        .width = static_cast<u32>(width),
        .height = static_cast<u32>(height),
        .bytes_per_pixel = load_info.desired_channels,
        .is_srgb = load_info.is_srgb,
        .gen_mip_maps = load_info.gen_mip_maps,
        .linear_filter = load_info.linear_filter,
    });

    stbi_image_free(pixels);

    return handle;
}
Handle<Texture> Renderer::create_u8_texture(const TextureCreateInfo &create_info) {
    if(!create_info.pixel_data) {
        DEBUG_PANIC("create_u8_texture failed! | create_info.pixel_data cannot be nullptr!")
    }

    if(create_info.width == 0U) {
        DEBUG_PANIC("create_u8_texture failed! | create_info.width must be greater than 0!")
    }
    if(create_info.height == 0U) {
        DEBUG_PANIC("create_u8_texture failed! | create_info.height must be greater than 0!")
    }

    VkFormat format{};
    if(create_info.bytes_per_pixel == 1U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM);
    } else if(create_info.bytes_per_pixel == 2U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM);
    } else if(create_info.bytes_per_pixel == 3U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM);
    } else if(create_info.bytes_per_pixel == 4U) {
        format = (create_info.is_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM);
    } else {
        DEBUG_PANIC("create_u8_texture failed! | create_info.bytes_per_pixel must be between 1 and 4, bytes_per_pixel=" << create_info.bytes_per_pixel)
    }

    DEBUG_ASSERT(m_config_texture_anisotropy <= 16U)

    Texture texture{
        .width = create_info.width,
        .height = create_info.height,
        .bytes_per_pixel = create_info.bytes_per_pixel,
        .mip_level_count = (create_info.gen_mip_maps ? Utils::calculate_mipmap_levels_xy(create_info.width, create_info.height) : 1U),
        .is_srgb = static_cast<u32>(create_info.is_srgb),
        .use_linear_filter = static_cast<u32>(create_info.linear_filter)
    };

    texture.image = m_api.m_resource_manager->create_image(ImageCreateInfo{
        .format = format,
        .extent {
            .width = texture.width,
            .height = texture.height
        },
        .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | (create_info.gen_mip_maps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : static_cast<VkImageUsageFlags>(0)),
        .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
        .mip_level_count = texture.mip_level_count
    });

    texture.sampler = m_api.m_resource_manager->create_sampler(SamplerCreateInfo{
        .filter = create_info.linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmap_mode = create_info.linear_filter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,

        .max_mipmap = static_cast<f32>(texture.mip_level_count),
        .mipmap_bias = m_config_texture_mip_bias,
        .anisotropy = static_cast<f32>(m_config_texture_anisotropy)
    });

    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = create_info.width * create_info.height * create_info.bytes_per_pixel,
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, create_info.pixel_data, create_info.width * create_info.height * create_info.bytes_per_pixel);
    m_api.record_and_submit_once([this, &create_info, &texture, staging_buffer](Handle<CommandList> cmd) {
        m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, {ImageBarrier{
            .image_handle = texture.image,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        }});

        m_api.copy_buffer_to_image(cmd, staging_buffer, texture.image, {BufferToImageCopy{}});

        if (create_info.gen_mip_maps) {
            m_api.gen_mipmaps(cmd, texture.image, create_info.linear_filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT
            );
        } else {
            m_api.image_barrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, {ImageBarrier{
                .image_handle = texture.image,
                .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
                .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }});
        }
    });
    m_api.m_resource_manager->destroy_buffer(staging_buffer);

    auto handle = m_texture_allocator.alloc(texture);

    m_api.m_resource_manager->update_descriptor(m_forward_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .array_index = static_cast<u32>(handle),
                .image_info {
                    .image_handle = texture.image,
                    .image_sampler = texture.sampler
                }
            }
        }
    });

    return handle;
}
void Renderer::destroy_texture(Handle<Texture> texture_handle) {
    if(!m_texture_allocator.is_handle_valid(texture_handle)) {
        DEBUG_PANIC("Cannot delete texture - Texture with a handle id: = " << texture_handle << ", does not exist!")
    }

    const Texture &texture = m_texture_allocator.get_element(texture_handle);
    m_api.m_resource_manager->destroy_image(texture.image);
    m_api.m_resource_manager->destroy_sampler(texture.sampler);

    m_texture_allocator.free(texture_handle);
}

Handle<Material> Renderer::create_material(const MaterialCreateInfo &create_info) {
    Material material{
        .albedo_texture = (create_info.albedo_texture == INVALID_HANDLE) ? m_default_white_srgb_texture : create_info.albedo_texture,
        .roughness_texture = (create_info.roughness_texture == INVALID_HANDLE) ? m_default_grey_unorm_texture : create_info.roughness_texture,
        .metalness_texture = (create_info.metalness_texture == INVALID_HANDLE) ? m_default_grey_unorm_texture : create_info.metalness_texture,
        .normal_texture = (create_info.normal_texture == INVALID_HANDLE) ? m_default_grey_unorm_texture : create_info.normal_texture,
        .color = create_info.color
    };

    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY
    // TEMPORARY TEMPORARY TEMPORARY TEMPORARY

    Handle<Buffer> staging_buffer = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Material),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    m_api.m_resource_manager->memcpy_to_buffer_once(staging_buffer, &material, sizeof(Material));

    auto handle = m_material_allocator.alloc(material);

    m_api.record_and_submit_once([this, handle, staging_buffer](Handle<CommandList> cmd){
        m_api.copy_buffer_to_buffer(cmd, staging_buffer, m_scene_material_buffer, {
            VkBufferCopy {
                .dstOffset = handle * sizeof(Material),
                .size = sizeof(Material),
            }
        });
    });

    m_api.m_resource_manager->destroy_buffer(staging_buffer);

    return handle;
}
void Renderer::destroy_material(Handle<Material> material_handle) {
    if(!m_material_allocator.is_handle_valid(material_handle)) {
        DEBUG_PANIC("Cannot delete material - Material with a handle id: = " << material_handle << ", does not exist!")
    }

    const Material& material = m_material_allocator.get_element(material_handle);

    m_material_allocator.free(material_handle);
}