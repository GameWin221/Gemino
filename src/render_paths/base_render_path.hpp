#ifndef GEMINO_BASE_RENDER_PATH_HPP
#define GEMINO_BASE_RENDER_PATH_HPP

#include <window/window.hpp>
#include <renderer/renderer.hpp>
#include <world/world.hpp>

#include <common/types.hpp>
#include <common/utils.hpp>
#include <common/debug.hpp>

class BaseRenderPath {
public:
    BaseRenderPath(const Window& window, const RendererConfig& render_config);
    ~BaseRenderPath();

    BaseRenderPath& operator=(const BaseRenderPath& other) = delete;
    BaseRenderPath& operator=(BaseRenderPath&& other) noexcept = delete;

    virtual void begin_recording_frame() = 0;
    virtual void end_recording_frame() = 0;

protected:
    Renderer renderer;
};

#endif
