#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <renderer/renderer.hpp>
#include <world/world.hpp>

class Editor {
public:
    static void attach(Renderer &renderer);
    static void detach(Renderer &renderer);

    static bool is_attached();

private:
    static void draw(Renderer &renderer, World &world);
    static void draw_gpu_memory_usage_window(Renderer &renderer, World &world);
};

#endif