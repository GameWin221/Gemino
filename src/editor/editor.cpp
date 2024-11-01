#include "editor.hpp"

#include "imgui.h"

void Editor::attach(Renderer &renderer) {
    renderer.m_ui_pass_draw_fn = [this] { draw(); };
}
void Editor::detach(Renderer &renderer) {
    renderer.m_ui_pass_draw_fn = nullptr;
}

void Editor::draw() {
    ImGui::ShowDemoWindow();
}
