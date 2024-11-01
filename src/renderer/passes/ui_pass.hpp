#ifndef UI_PASS_HPP
#define UI_PASS_HPP

#include <window/window.hpp>
#include <render_api/render_api.hpp>

class Renderer;
class World;

typedef void (*UIPassDrawFn)(Renderer &renderer, World &world);

class UIPass {
public:
    void init(const RenderAPI &api, const Window &window);
    void destroy(const RenderAPI &api);
    void resize(const RenderAPI &api, const Window &window);

    void process(const RenderAPI &api, Handle<CommandList> command_list, UIPassDrawFn draw_fn, u32 swapchain_target_index, Renderer &renderer, World &world);
private:

    VkExtent2D m_extent{};
    VkRenderPass m_render_pass{};

    std::vector<VkFramebuffer> m_framebuffers{};
};

#endif