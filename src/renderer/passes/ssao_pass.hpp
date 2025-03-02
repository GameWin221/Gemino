#ifndef SSAO_PASS_HPP
#define SSAO_PASS_HPP

#include <renderer/base_pass.hpp>

struct SSAOPushConstant {
    i32 screen_wh_combined{};
    f32 radius{};
    f32 bias{};
    f32 multiplier{};
    f32 noise_scale{};
};

class SSAOPass : public BasePass {
public:
    void init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void destroy(const RenderAPI &api) override;
    void process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) override;

private:
    Handle<RenderTarget> m_render_target{};
    Handle<GraphicsPipeline> m_pipeline{};
    Handle<Descriptor> m_descriptor{};
};

#endif
