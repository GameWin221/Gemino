#ifndef GEOMETRY_PASS_HPP
#define GEOMETRY_PASS_HPP

#include <renderer/base_pass.hpp>

class GeometryPass : public BasePass {
public:
    void init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void destroy(const RenderAPI &api) override;
    void process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) override;

private:
    Handle<RenderTarget> m_render_target{};
    Handle<Descriptor> m_descriptor{};
    Handle<GraphicsPipeline> m_pipeline{};
};

#endif
