#ifndef GEOMETRY_PASS_HPP
#define GEOMETRY_PASS_HPP

#include <render_api/render_api.hpp>
#include <render_api/managers/pipeline_manager.hpp>

class GeometryPass {
public:
    void init(
        const RenderAPI &api,
        Handle<Buffer> scene_draw_buffer,
        Handle<Buffer> scene_object_buffer,
        Handle<Buffer> scene_global_transform_buffer,
        Handle<Buffer> scene_material_buffer,
        Handle<Buffer> scene_mesh_instance_buffer,
        Handle<Buffer> scene_mesh_instance_materials_buffer,
        Handle<Buffer> scene_camera_buffer,
        Handle<Buffer> scene_vertex_buffer,
        Handle<Image> offscreen_image,
        Handle<Image> depth_image,
        Handle<Descriptor> scene_texture_descriptor
    );
    void destroy(const RenderAPI &api);
    void resize(const RenderAPI &api, Handle<Image> offscreen_image, Handle<Image> depth_image);

    void process(
        const RenderAPI &api,
        Handle<CommandList> cmd,
        Handle<Image> offscreen_image,
        Handle<Descriptor> scene_texture_descriptor,
        Handle<Buffer> scene_index_buffer,
        Handle<Buffer> scene_draw_buffer,
        Handle<Buffer> scene_draw_count_buffer,
        u32 max_draws,
        u32 stride
    );
private:
    Handle<RenderTarget> m_render_target{};
    Handle<Descriptor> m_descriptor{};
    Handle<GraphicsPipeline> m_pipeline{};
};

#endif
