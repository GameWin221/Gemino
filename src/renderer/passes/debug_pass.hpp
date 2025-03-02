#ifndef DEBUG_PASS_HPP
#define DEBUG_PASS_HPP

#include <renderer/base_pass.hpp>

class DebugPass : public BasePass {
public:
    void init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void destroy(const RenderAPI &api) override;
    void process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) override;

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
