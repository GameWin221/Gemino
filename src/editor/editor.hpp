#ifndef EDITOR_HPP
#define EDITOR_HPP

#include <renderer/renderer.hpp>

class Editor {
public:
    void attach(Renderer &renderer);
    void detach(Renderer &renderer);

private:
    void draw();
};

#endif