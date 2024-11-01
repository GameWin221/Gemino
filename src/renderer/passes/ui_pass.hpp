#ifndef UI_PASS_HPP
#define UI_PASS_HPP

#include <window/window.hpp>
#include <render_api/render_api.hpp>

class UIPass {
public:
    void init(const RenderAPI &api, const Window &window);
    void destroy(const RenderAPI &api);
    void resize(const RenderAPI &api, const Window &window);

    void process(const RenderAPI &api, Handle<CommandList> command_list, const std::function<void()> &draw_fn, u32 swapchain_target_index);
private:
    void create_framebuffers();
    void destroy_framebuffers();

    VkExtent2D m_extent{};
    VkRenderPass m_render_pass{};

    std::vector<VkFramebuffer> m_framebuffers{};
};

#endif