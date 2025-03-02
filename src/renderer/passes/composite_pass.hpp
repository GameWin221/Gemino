#ifndef COMPOSITE_PASS_HPP
#define COMPOSITE_PASS_HPP

#include <renderer/base_pass.hpp>

class CompositePass : public BasePass {
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
