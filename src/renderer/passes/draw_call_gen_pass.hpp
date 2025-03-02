#ifndef DRAW_CALL_GEN_PASS_HPP
#define DRAW_CALL_GEN_PASS_HPP

#include <renderer/base_pass.hpp>

struct DrawCallGenPushConstant {
    u32 object_count_pre_cull{};
    f32 global_lod_bias{};
    f32 global_cull_dist_multiplier{};
    f32 lod_sphere_visible_angle{};
};

class DrawCallGenPass : public BasePass {
public:
    void init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void destroy(const RenderAPI &api) override;
    void process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) override;

private:
    Handle<Descriptor> m_descriptor{};
    Handle<ComputePipeline> m_pipeline{};
};

#endif