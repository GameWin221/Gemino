#ifndef DEBUG_PASS_HPP
#define DEBUG_PASS_HPP

#include <render_api/render_api.hpp>
#include <render_api/managers/pipeline_manager.hpp>

class DebugPass {
public:
    void init(
        const RenderAPI &api,
        Handle<Buffer> scene_draw_buffer,
        Handle<Buffer> scene_object_buffer,
        Handle<Buffer> scene_global_transform_buffer,
        Handle<Buffer> scene_mesh_buffer,
        Handle<Buffer> scene_mesh_instance_buffer,
        Handle<Buffer> scene_camera_buffer,
        Handle<Buffer> scene_draw_count_buffer,
        Handle<Image> offscreen_image,
        Handle<Image> depth_image
    );
    void destroy(const RenderAPI &api);
    void resize(const RenderAPI &api, Handle<Image> offscreen_image, Handle<Image> depth_image);

    void process(const RenderAPI &api, Handle<CommandList> cmd, f32 debug_shape_opacity);
private:
    Handle<Descriptor> m_graphics_descriptor{};
    Handle<GraphicsPipeline> m_graphics_pipeline{};
    Handle<RenderTarget> m_render_target{};

    Handle<Descriptor> m_compute_descriptor{};
    Handle<ComputePipeline> m_compute_pipeline{};

    Handle<Buffer> m_sphere_indirect_draw_buffer{};
    Handle<Buffer> m_sphere_mesh_vertex_buffer{};
    Handle<Buffer> m_sphere_mesh_index_buffer{};

    const u32 VERTICES_PER_AXIS = (1u << 5u);
};

#endif
