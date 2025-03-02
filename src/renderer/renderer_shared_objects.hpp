#ifndef GEMINO_RENDERER_SHARED_OBJECTS_HPP
#define GEMINO_RENDERER_SHARED_OBJECTS_HPP

#include <RHI/resource_manager.hpp>

class Renderer;
class World;
struct RendererSharedObjects;

typedef void (*UIPassDrawFn)(World &world);

struct RendererSharedObjects {
    // Config start
    float config_global_lod_bias{};
    float config_global_cull_dist_multiplier = 1.0f;
    bool config_enable_dynamic_lod = true;
    bool config_enable_frustum_cull = true;
    bool config_enable_debug_shape_view = false;
    f32 config_debug_shape_opacity = 0.3f;
    f32 config_lod_sphere_visible_angle = 0.01f;

    u32 config_texture_anisotropy = 8U;
    float config_texture_mip_bias = 0.0f;
    // Config end

    UIPassDrawFn ui_pass_draw_fn{};

    u32 frames_since_init{};
    u32 swapchain_target_index{};

    Handle<Image> offscreen_image{};
    Handle<Sampler> offscreen_sampler{};
    Handle<Image> depth_image{};

    Handle<Material> default_material{};
    Handle<Texture> default_white_srgb_texture{};
    Handle<Texture> default_grey_unorm_texture{};

    Handle<Descriptor> scene_texture_descriptor{};
    Handle<Buffer> scene_vertex_buffer{};
    Handle<Buffer> scene_index_buffer{};
    Handle<Buffer> scene_primitive_buffer{};
    Handle<Buffer> scene_mesh_buffer{};
    Handle<Buffer> scene_mesh_instance_buffer{};
    Handle<Buffer> scene_mesh_instance_materials_buffer{};
    Handle<Buffer> scene_object_buffer{};
    Handle<Buffer> scene_global_transform_buffer{};
    Handle<Buffer> scene_material_buffer{};
    Handle<Buffer> scene_camera_buffer{};
    Handle<Buffer> scene_draw_buffer{};
    Handle<Buffer> scene_draw_count_buffer{};
};

#endif