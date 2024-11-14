#ifndef DRAW_CALL_GEN_PASS_HPP
#define DRAW_CALL_GEN_PASS_HPP

#include <render_api/render_api.hpp>
#include <render_api/managers/pipeline_manager.hpp>

struct DrawCallGenPushConstant {
    u32 object_count_pre_cull{};
    f32 global_lod_bias{};
    f32 global_cull_dist_multiplier{};
    f32 lod_sphere_visible_angle{};
};

class DrawCallGenPass {
public:
    void init(
        const RenderAPI &api,
        Handle<Buffer> scene_mesh_buffer,
        Handle<Buffer> scene_primitive_buffer,
        Handle<Buffer> scene_draw_buffer,
        Handle<Buffer> scene_object_buffer,
        Handle<Buffer> scene_global_transform_buffer,
        Handle<Buffer> scene_mesh_instance_buffer,
        Handle<Buffer> scene_draw_count_buffer,
        Handle<Buffer> scene_camera_buffer,
        bool config_enable_dynamic_lod,
        bool config_enable_frustum_cull
    );
    void destroy(const RenderAPI &api);
    void resize(const RenderAPI &api);

    void process(
        const RenderAPI &api,
        Handle<CommandList> cmd,
        Handle<Buffer> scene_draw_buffer,
        Handle<Buffer> scene_draw_count_buffer,
        u32 scene_objects_count,
        f32 config_global_lod_bias,
        f32 config_global_cull_dist_multiplier,
        f32 config_lod_sphere_visible_angle
        );
private:
    Handle<Descriptor> m_descriptor{};
    Handle<ComputePipeline> m_pipeline{};
};

#endif