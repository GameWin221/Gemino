#include "renderer.hpp"

#include <stb/stb_image.h>

#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>

static glm::vec3 calculate_center_offset(const Vertex *vertices, u32 vertex_count) {
    glm::vec3 center_offset{};
    for(u32 i{}; i < vertex_count; ++i) {
        center_offset += vertices[i].pos;
    }

    center_offset /= static_cast<f32>(vertex_count);

    return center_offset;
}
static f32 calculate_radius(const Vertex *vertices, u32 vertex_count, const glm::vec3 &center_offset) {
    f32 max_dist{};
    for(u32 i{}; i < vertex_count; ++i) {
        f32 dist = glm::distance(center_offset, vertices[i].pos);

        if(dist > max_dist) {
            max_dist = dist;
        }
    }

    return max_dist;
}

static void process_gltf_node(SceneCreateInfo &scene, const tinygltf::Model &model, u32 node_id) {
    const tinygltf::Node &node = model.nodes[node_id];

    ObjectCreateInfo object{};
    object.name = node.name;

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

    if (node.mesh != -1) {
        object.mesh_instance = scene.mesh_instances[node.mesh];
    }

    u32 object_id = static_cast<u32>(scene.objects.size());
    scene.objects.push_back(object);
    scene.children.emplace_back();

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

    // Set dummy image loader callback so that tinygltf doesn't complain
    loader.SetImageLoader([](tinygltf::Image *, const i32, std::string *, std::string *, i32, i32, const uint8_t *, i32, void *) {
        return true;
    }, nullptr);

    loader.LoadASCIIFromFile(&model, &err, &warn, load_info.path);

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

    if (load_info.import_textures && load_info.import_materials) {
        scene.textures.resize(model.textures.size());

        std::vector<bool> is_texture_srgb(model.textures.size(), false);
        for (const auto &material : model.materials) {
            i32 albedo_texture_id = material.pbrMetallicRoughness.baseColorTexture.index;

            // Only albedo should be sRGB
            if (albedo_texture_id != -1) {
                is_texture_srgb[albedo_texture_id] = true;
            }
        }

        for (u32 texture_id{}; texture_id < static_cast<u32>(model.textures.size()); ++texture_id) {
            const auto &texture = model.textures[texture_id];
            const auto &image = model.images[texture.source];
            const auto &sampler = model.samplers[texture.sampler];

            // sampler.magFilter = TINYGLTF_TEXTURE_FILTER_NEAREST | TINYGLTF_TEXTURE_FILTER_LINEAR
            // sampler.minFilter = TINYGLTF_TEXTURE_FILTER_NEAREST | TINYGLTF_TEXTURE_FILTER_LINEAR | TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST |
            // TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST | TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR | TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR

            // sampler.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT | TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE | TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT
            // sampler.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT | TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE | TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT

            if (!image.uri.empty()) {
                std::string directory = Utils::get_directory(load_info.path);

                scene.textures[texture_id] = load_u8_texture(TextureLoadInfo{
                    .path = directory + image.uri,
                    .is_srgb = is_texture_srgb[texture_id],
                    .gen_mip_maps = true
                });
            } else if (image.bufferView != -1) {
                const auto &buffer_view = model.bufferViews[image.bufferView];
                const auto &buffer = model.buffers[buffer_view.buffer];

                i32 width, height, channels;
                u8 *pixels = stbi_load_from_memory(
                    reinterpret_cast<stbi_uc const *>(reinterpret_cast<usize>(buffer.data.data()) + buffer_view.byteOffset),
                    static_cast<i32>(buffer_view.byteLength),
                    &width,
                    &height,
                    &channels,
                    4u
                );

                if(!pixels) {
                    DEBUG_WARNING("LEN: " << static_cast<i32>(buffer.data.size()))
                    DEBUG_PANIC("Failed to load image from buffer[" << image.bufferView << "]")
                }

                DEBUG_LOG("Loaded image \"" << image.name << "\" from buffer[" << buffer_view.buffer << "], bufferView[" << image.bufferView << "]")

                scene.textures[texture_id] = create_u8_texture(TextureCreateInfo{
                    .pixel_data = pixels,
                    .width = static_cast<u32>(width),
                    .height = static_cast<u32>(height),
                    .bytes_per_pixel = 4u,
                    .is_srgb = is_texture_srgb[texture_id],
                    .gen_mip_maps = true
                });

                stbi_image_free(pixels);
            } else {
                DEBUG_PANIC("Failed to import image! No source URI or bufferView provided!")
            }
        }
    }

    scene.materials.resize(model.materials.size(), m_default_material);
    if (load_info.import_materials) {
        for (u32 material_id{}; material_id < static_cast<u32>(model.materials.size()); ++material_id) {
            const auto &material = model.materials[material_id];

            i32 albedo_texture_id = material.pbrMetallicRoughness.baseColorTexture.index;
            i32 roughness_texture_id = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            i32 metalness_texture_id = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            i32 normal_texture_id = material.normalTexture.index;

            //.name = material.name;
            scene.materials[material_id] = create_material(MaterialCreateInfo {
                .albedo_texture = ((albedo_texture_id != -1 && load_info.import_textures) ? scene.textures[albedo_texture_id] : INVALID_HANDLE),
                .roughness_texture = ((roughness_texture_id != -1 && load_info.import_textures) ? scene.textures[roughness_texture_id] : INVALID_HANDLE),
                .metalness_texture = ((metalness_texture_id != -1 && load_info.import_textures) ? scene.textures[metalness_texture_id] : INVALID_HANDLE),
                .normal_texture = ((normal_texture_id != -1 && load_info.import_textures) ? scene.textures[normal_texture_id] : INVALID_HANDLE),
                //.metalness_factor = static_cast<f32>(material.pbrMetallicRoughness.metallicFactor),
                //.roughness_factor = static_cast<f32>(material.pbrMetallicRoughness.roughnessFactor),
                .color = glm::vec4(
                    material.pbrMetallicRoughness.baseColorFactor[0],
                    material.pbrMetallicRoughness.baseColorFactor[1],
                    material.pbrMetallicRoughness.baseColorFactor[2],
                    material.pbrMetallicRoughness.baseColorFactor[3]
                )
            });

            DEBUG_LOG("Loaded material \"" << material.name << "\" from \"" << load_info.path << "\"")
        }
    }

    scene.meshes.resize(model.meshes.size());
    scene.mesh_instances.resize(model.meshes.size());
    for (uint32_t mesh_id{}; mesh_id < static_cast<u32>(model.meshes.size()); ++mesh_id) {
        const auto &mesh = model.meshes[mesh_id];

        MeshCreateInfo mesh_create_info{};

        std::vector<std::vector<Vertex>> mesh_vertices(mesh.primitives.size());
        std::vector<std::vector<u32>> mesh_indices(mesh.primitives.size());

        std::vector<Handle<Material>> primitives_default_materials(mesh.primitives.size());
        for(uint32_t primitive_id{}; primitive_id < static_cast<u32>(mesh.primitives.size()); ++primitive_id) {
            const auto &primitive = mesh.primitives[primitive_id];
            //.name = mesh.name + std::string("_") + std::to_string(primitive_id++);

            DEBUG_ASSERT(primitive.mode == TINYGLTF_MODE_TRIANGLES);

            std::vector<glm::vec3> positions{};
            std::vector<glm::vec3> normals{};
            std::vector<glm::vec2> texcoords{};

            for (const auto &[attrib_name, attrib_id] : primitive.attributes) {
                const tinygltf::Accessor &accessor = model.accessors[attrib_id];
                const tinygltf::BufferView &buffer_view = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = model.buffers[buffer_view.buffer];

                if (attrib_name == "POSITION") {
                    if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        positions.resize(accessor.count);

                        DEBUG_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3)
                        usize data_size = accessor.count * sizeof(float) * 3;

                        std::memcpy(positions.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(buffer.data.data()) + accessor.byteOffset + buffer_view.byteOffset), data_size);
                    } else {
                        DEBUG_PANIC("Unsupported gltf POSITION attribute component type for mesh \"" << mesh.name << "\"")
                    }
                } else if (attrib_name == "NORMAL") {
                    if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        normals.resize(accessor.count);

                        DEBUG_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3)

                        usize data_size = accessor.count * sizeof(float) * 3;

                        std::memcpy(normals.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(buffer.data.data()) + accessor.byteOffset + buffer_view.byteOffset), data_size);
                    } else {
                        DEBUG_PANIC("Unsupported gltf NORMAL attribute component type for mesh \"" << mesh.name << "\"")
                    }
                } else if (attrib_name == "TEXCOORD_0") {
                    if(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        texcoords.resize(accessor.count);

                        DEBUG_ASSERT(accessor.type == TINYGLTF_TYPE_VEC2)

                        usize data_size = accessor.count * sizeof(float) * 2;

                        std::memcpy(texcoords.data(), reinterpret_cast<void*>(reinterpret_cast<usize>(buffer.data.data()) + accessor.byteOffset + buffer_view.byteOffset), data_size);
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

            mesh_vertices[primitive_id].resize(positions.size());

            for(usize j{}; j < positions.size(); ++j) {
                mesh_vertices[primitive_id][j].pos = positions[j];
                mesh_vertices[primitive_id][j].normal = glm::i8vec4(glm::normalize(normals[j]) * 127.0f, 0.0f);

                mesh_vertices[primitive_id][j].texcoord = glm::u16vec2(Utils::f32_to_f16(texcoords[j].x), Utils::f32_to_f16(texcoords[j].y));
            }


            const tinygltf::Accessor &index_accessor = model.accessors[primitive.indices];
            const tinygltf::BufferView &index_buffer_view = model.bufferViews[index_accessor.bufferView];
            const tinygltf::Buffer &index_buffer = model.buffers[index_buffer_view.buffer];

            mesh_indices[primitive_id].resize(index_accessor.count);

            if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                for(usize offset = index_accessor.byteOffset + index_buffer_view.byteOffset, j{}; j < index_accessor.count; offset += sizeof(u8), ++j) {
                    mesh_indices[primitive_id][j] = static_cast<u32>(*reinterpret_cast<u8*>(reinterpret_cast<usize>(index_buffer.data.data()) + offset));
                }
            } else if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                for(usize offset = index_accessor.byteOffset + index_buffer_view.byteOffset, j{}; j < index_accessor.count; offset += sizeof(u16), ++j) {
                    mesh_indices[primitive_id][j] = static_cast<u32>(*reinterpret_cast<u16*>(reinterpret_cast<usize>(index_buffer.data.data()) + offset));
                }
            } else if(index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                usize data_size = index_accessor.count * sizeof(u32);
                std::memcpy(mesh_indices[primitive_id].data(), reinterpret_cast<void*>(reinterpret_cast<usize>(index_buffer.data.data()) + index_accessor.byteOffset + index_buffer_view.byteOffset), data_size);
            } else {
                DEBUG_PANIC("Unsupported gltf indices component type for mesh \"" << mesh.name << "\"")
            }

            if (primitive.material >= 0) {
                primitives_default_materials[primitive_id] = scene.materials[primitive.material];
            } else {
                primitives_default_materials[primitive_id] = m_default_material;
            }

            mesh_create_info.primitives.push_back(PrimitiveCreateInfo{
                .vertex_data = mesh_vertices[primitive_id].data(),
                .vertex_count = static_cast<u32>(mesh_vertices[primitive_id].size()),
                .index_data = mesh_indices[primitive_id].data(),
                .index_count = static_cast<u32>(mesh_indices[primitive_id].size())
            });
        }

        scene.meshes[mesh_id] = create_mesh(mesh_create_info);
        scene.mesh_instances[mesh_id] = create_mesh_instance(MeshInstanceCreateInfo{
            .mesh = scene.meshes[mesh_id],
             .materials = primitives_default_materials
        });

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
    Range<Primitive> primitive_range = m_primitive_allocator.alloc(static_cast<u32>(create_info.primitives.size()));
    std::vector<glm::vec4> primitive_bounding_spheres(primitive_range.count);

    for (u32 primitive_id{}; primitive_id < primitive_range.count; ++primitive_id) {
        const auto &primitive_info = create_info.primitives[primitive_id];

        // TODO: Implement automatic LOD creation
        Primitive primitive_data{};

        Range<Vertex> vertex_range = m_vertex_allocator.alloc(primitive_info.vertex_count);
        Range<u32> index_range = m_index_allocator.alloc(primitive_info.index_count);

        primitive_data.lods[0u] = PrimitiveLOD {
            .index_start = index_range.start,
            .index_count = index_range.count,
            .vertex_start = static_cast<i32>(vertex_range.start),
            .vertex_count = vertex_range.count
        };
        for(u32 i = 1u; i < static_cast<u32>(primitive_data.lods.size()); ++i) {
            primitive_data.lods[i] = primitive_data.lods[0];
        }

        m_primitive_allocator.get_element_mutable(primitive_range.start + primitive_id) = primitive_data;

        glm::vec3 center_offset = calculate_center_offset(primitive_info.vertex_data, vertex_range.count);
        f32 radius = calculate_radius(primitive_info.vertex_data, vertex_range.count, center_offset);
        primitive_bounding_spheres[primitive_id] = glm::vec4(center_offset.x, center_offset.y, center_offset.z, radius);

        // When I eventually add LODs, then I'll have to sum the total amount of vertices and indices of each LOD
        Handle<Buffer> staging = m_api.m_resource_manager->create_buffer(BufferCreateInfo {
            .size = vertex_range.count * sizeof(Vertex) + index_range.count * sizeof(u32) + sizeof(Primitive),
            .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
        });
        void *mapped_staging = m_api.m_resource_manager->map_buffer(staging);

        std::vector<VkBufferCopy> vertex_copy_regions{};
        std::vector<VkBufferCopy> index_copy_regions{};
        VkBufferCopy primitive_copy_region{};

        usize dst_offset = 0ull;
        m_api.m_resource_manager->memcpy_to_buffer(mapped_staging, primitive_info.vertex_data, sizeof(Vertex) * static_cast<usize>(vertex_range.count), dst_offset);
        vertex_copy_regions.push_back(VkBufferCopy{
            .srcOffset = dst_offset,
            .dstOffset = vertex_range.start * sizeof(Vertex),
            .size = sizeof(Vertex) * static_cast<usize>(vertex_range.count)
        });

        dst_offset += sizeof(Vertex) * static_cast<usize>(vertex_range.count);
        m_api.m_resource_manager->memcpy_to_buffer(mapped_staging, primitive_info.index_data, sizeof(u32) * static_cast<usize>(index_range.count), dst_offset);
        index_copy_regions.push_back(VkBufferCopy{
            .srcOffset = dst_offset,
            .dstOffset = index_range.start * sizeof(u32),
            .size = sizeof(u32) * static_cast<usize>(index_range.count)
        });

        dst_offset += sizeof(u32) * static_cast<usize>(index_range.count);
        m_api.m_resource_manager->memcpy_to_buffer(mapped_staging, &primitive_data, sizeof(primitive_data), dst_offset);
        primitive_copy_region = VkBufferCopy {
            .srcOffset = dst_offset,
            .dstOffset = (primitive_range.start + primitive_id) * sizeof(Primitive),
            .size = sizeof(Primitive)
        };

        dst_offset += sizeof(Primitive);

        m_api.m_resource_manager->unmap_buffer(staging);

        m_api.record_and_submit_once([this, staging, &vertex_copy_regions, &index_copy_regions, &primitive_copy_region](Handle<CommandList> cmd) {
            m_api.copy_buffer_to_buffer(cmd, staging, m_scene_vertex_buffer, vertex_copy_regions);
            m_api.copy_buffer_to_buffer(cmd, staging, m_scene_index_buffer, index_copy_regions);
            m_api.copy_buffer_to_buffer(cmd, staging, m_scene_primitive_buffer, { primitive_copy_region });
        });

        m_api.m_resource_manager->destroy_buffer(staging);
    }

    glm::vec3 avg_center{};
    for(const auto &bound : primitive_bounding_spheres) {
        avg_center += glm::vec3(bound.x, bound.y, bound.z);
    }
    avg_center /= static_cast<f32>(primitive_range.count);

    f32 max_radius{};
    for(const auto &bound : primitive_bounding_spheres) {
        glm::vec3 point = glm::vec3(bound);
        f32 radius = bound.w;

        f32 d = glm::distance(avg_center, point) + radius;
        if (d > max_radius) {
            max_radius = d;
        }
    }

    // Register mesh
    Mesh mesh{
        .center_offset = avg_center,
        .radius = max_radius,
        .primitive_count = primitive_range.count,
        .primitive_start = primitive_range.start
    };

    Handle<Mesh> mesh_handle = m_mesh_allocator.alloc(mesh);

    Handle<Buffer> staging = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = sizeof(Mesh),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    m_api.m_resource_manager->memcpy_to_buffer_once(staging, &mesh, sizeof(mesh));

    VkBufferCopy mesh_buffer_copy {
        .dstOffset = mesh_handle * sizeof(Mesh),
        .size = sizeof(Mesh)
    };

    m_api.record_and_submit_once([this, staging, &mesh_buffer_copy](Handle<CommandList> cmd) {
       m_api.copy_buffer_to_buffer(cmd, staging, m_scene_mesh_buffer, { mesh_buffer_copy });
    });

    m_api.m_resource_manager->destroy_buffer(staging);

    return mesh_handle;
}
void Renderer::destroy_mesh(Handle<Mesh> mesh_handle) {
    if(!m_mesh_allocator.is_handle_valid(mesh_handle)) {
        DEBUG_PANIC("Cannot delete mesh - Mesh with a handle id: = " << mesh_handle << ", does not exist!")
    }

    const Mesh &mesh = m_mesh_allocator.get_element(mesh_handle);

    Range<Primitive> primitive_range{ mesh.primitive_start, mesh.primitive_count };
    if(!m_primitive_allocator.is_range_valid(primitive_range)) {
        DEBUG_PANIC("Cannot delete primitives - Primitives at Range{ start: " << primitive_range.start << ", count: " << primitive_range.count << " }, does not exist!")
    }

    for (u32 i{}; i < primitive_range.count; ++i) {
        const Primitive &prim = m_primitive_allocator.get_element(primitive_range.start + i);
        for (u32 lod_id{}; lod_id < static_cast<u32>(prim.lods.size()); ++lod_id) {
            const PrimitiveLOD &lod = prim.lods[lod_id];
            m_vertex_allocator.free(Range<Vertex>{static_cast<u32>(lod.vertex_start), lod.vertex_count});
            m_index_allocator.free(Range<u32>{lod.index_start, lod.index_count});
        }
    }

    m_mesh_allocator.free(mesh_handle);
    m_primitive_allocator.free(primitive_range);
}

Handle<MeshInstance> Renderer::create_mesh_instance(const MeshInstanceCreateInfo &create_info) {
    Range<Handle<Material>> material_range = m_mesh_instance_materials_allocator.alloc(static_cast<u32>(create_info.materials.size()), create_info.materials.data());

    const Mesh &mesh_data = m_mesh_allocator.get_element(create_info.mesh);
    if (mesh_data.primitive_count != material_range.count) {
        DEBUG_PANIC("Failed to create a MeshInstance! create_info.materials.size() must be equal to primitive_count of the specified mesh.");
    }

    MeshInstance instance{
        .mesh = create_info.mesh,
        .material_count = material_range.count,
        .material_start = material_range.start
    };
    
    Handle<Buffer> staging = m_api.m_resource_manager->create_buffer(BufferCreateInfo{
        .size = material_range.count * sizeof(Handle<Material>) + sizeof(MeshInstance),
        .buffer_usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .memory_usage_flags = VMA_MEMORY_USAGE_CPU_TO_GPU
    });

    Handle<MeshInstance> instance_handle = m_mesh_instance_allocator.alloc(instance);

    void *mapped_staging = m_api.m_resource_manager->map_buffer(staging);

    usize dst_offset{};
    m_api.m_resource_manager->memcpy_to_buffer(mapped_staging, create_info.materials.data(), material_range.count * sizeof(Handle<Material>), dst_offset);
    VkBufferCopy material_copy {
        .srcOffset = dst_offset,
        .dstOffset = material_range.start * sizeof(Handle<Material>),
        .size = material_range.count * sizeof(Handle<Material>)
    };

    dst_offset += material_range.count * sizeof(Handle<Material>);

    m_api.m_resource_manager->memcpy_to_buffer(mapped_staging, &instance, sizeof(MeshInstance), dst_offset);
    VkBufferCopy mesh_instance_copy {
        .srcOffset = dst_offset,
        .dstOffset = instance_handle * sizeof(MeshInstance),
        .size = sizeof(MeshInstance)
    };

    dst_offset += sizeof(MeshInstance);

    m_api.m_resource_manager->unmap_buffer(staging);

    m_api.record_and_submit_once([this, staging, &material_copy, &mesh_instance_copy](Handle<CommandList> cmd) {
       m_api.copy_buffer_to_buffer(cmd, staging, m_scene_mesh_instance_materials_buffer, { material_copy });
       m_api.copy_buffer_to_buffer(cmd, staging, m_scene_mesh_instance_buffer, { mesh_instance_copy });
    });

    m_api.m_resource_manager->destroy_buffer(staging);

    return instance_handle;
}
void Renderer::destroy_mesh_instance(Handle<MeshInstance> mesh_instance_handle) {
    if(!m_mesh_instance_allocator.is_handle_valid(mesh_instance_handle)) {
        DEBUG_PANIC("Cannot delete mesh instance - Mesh instance with a handle id: " << mesh_instance_handle << ", does not exist!")
    }

    const auto &mesh_instance = m_mesh_instance_allocator.get_element(mesh_instance_handle);

    Range<Handle<Material>> mat_range{ mesh_instance.material_start, mesh_instance.material_count };

    if(!m_mesh_instance_materials_allocator.is_range_valid(mat_range)) {
        DEBUG_PANIC("Cannot delete mesh instance materials - Mesh instance materials at Range{ start: " << mat_range.start << ", count: " << mat_range.count << " }, does not exist!")
    }

    m_mesh_instance_allocator.free(mesh_instance_handle);
    m_mesh_instance_materials_allocator.free(mat_range);
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
        .width = static_cast<u16>(create_info.width),
        .height = static_cast<u16>(create_info.height),
        .bytes_per_pixel = static_cast<u16>(create_info.bytes_per_pixel),
        .mip_level_count = static_cast<u16>(create_info.gen_mip_maps ? Utils::calculate_mipmap_levels_xy(create_info.width, create_info.height) : 1U),
        .is_srgb = static_cast<u16>(create_info.is_srgb),
        .use_linear_filter = static_cast<u16>(create_info.linear_filter)
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

    m_api.m_resource_manager->update_descriptor(m_scene_texture_descriptor, DescriptorUpdateInfo{
        .bindings{
            DescriptorBindingUpdateInfo{
                .binding_index = 0U,
                .array_index = handle,
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
        DEBUG_PANIC("Cannot delete texture - Texture with a handle id: " << texture_handle << ", does not exist!")
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
        DEBUG_PANIC("Cannot delete material - Material with a handle id: " << material_handle << ", does not exist!")
    }

    m_material_allocator.free(material_handle);
}