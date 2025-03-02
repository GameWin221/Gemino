#ifndef UI_PASS_HPP
#define UI_PASS_HPP

#include <renderer/base_pass.hpp>

class Renderer;
class World;

class UIPass : public BasePass {
public:
    void init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) override;
    void destroy(const RenderAPI &api) override;
    void process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) override;

private:

    VkExtent2D m_extent{};
    VkRenderPass m_render_pass{};

    std::vector<VkFramebuffer> m_framebuffers{};
};

#endif