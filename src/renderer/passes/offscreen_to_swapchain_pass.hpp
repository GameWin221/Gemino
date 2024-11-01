#ifndef OFFSCREEN_TO_SWAPCHAIN_PASS_HPP
#define OFFSCREEN_TO_SWAPCHAIN_PASS_HPP

#include <render_api/render_api.hpp>
#include <render_api/managers/pipeline_manager.hpp>

class OffscreenToSwapchainPass {
public:
    void init(const RenderAPI &api, Handle<Image> offscreen_image, Handle<Sampler> offscreen_sampler);
    void destroy(const RenderAPI &api);
    void resize(const RenderAPI &api, Handle<Image> offscreen_image, Handle<Sampler> offscreen_sampler);

    void process(const RenderAPI &api, Handle<CommandList> cmd, Handle<Image> offscreen_image, u32 swapchain_target_index);
private:
    Handle<GraphicsPipeline> m_pipeline{};
    Handle<Descriptor> m_descriptor{};
    std::vector<Handle<RenderTarget>> m_render_targets{};
};

#endif
