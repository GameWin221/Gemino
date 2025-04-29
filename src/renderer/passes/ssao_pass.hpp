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
struct SSAOBlurPushConstant {
    i32 blur_dir{};
    i32 screen_wh_combined{};
    f32 blur_radius{};
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

    Handle<Image> m_pingpong_image{};
    Handle<GraphicsPipeline> m_blur_pipeline{};
    Handle<RenderTarget> m_blur_rts[2]{};
    Handle<Descriptor> m_blur_descriptors[2]{};
};

#endif
