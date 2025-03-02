#ifndef BASE_PASS_HPP
#define BASE_PASS_HPP

#include <RHI/render_api.hpp>
#include <RHI/resource_manager.hpp>
#include <world/world.hpp>
#include <renderer/renderer_shared_objects.hpp>

class BasePass {
public:
    virtual ~BasePass() = default;

    virtual void init(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) = 0;
    virtual void resize(const RenderAPI &api, const RendererSharedObjects &shared, const Window &window) = 0;
    virtual void destroy(const RenderAPI &api) = 0;

    virtual void process(Handle<CommandList> cmd, const RenderAPI &api, const RendererSharedObjects &shared, const World &world) = 0;
};

#endif
