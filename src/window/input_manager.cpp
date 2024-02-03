#include "input_manager.hpp"

#include <unordered_map>
#include <GLFW/glfw3.h>

static std::unordered_map<GLFWwindow*, InputManager*> alive_managers{};

InputManager::InputManager(Window& polled_window) : polled_window_handle(static_cast<GLFWwindow*>(polled_window.get_native_handle())) {
    alive_managers[polled_window_handle] = this;

    glfwSetScrollCallback(polled_window_handle, InputManager::scroll_callback);
    glfwSetKeyCallback(polled_window_handle, InputManager::key_callback);
    glfwSetMouseButtonCallback(polled_window_handle, InputManager::mouse_button_callback);

    f64 mouse_x, mouse_y;
    glfwGetCursorPos(polled_window_handle, &mouse_x, &mouse_y);

    current_mouse_pos = glm::vec2(static_cast<f32>(mouse_x), static_cast<f32>(mouse_y));
    last_mouse_pos = current_mouse_pos;
}
InputManager::~InputManager() {
    alive_managers.erase(polled_window_handle);
}

void InputManager::poll_input() {
    scroll_delta = glm::vec2(0.0f);

    f64 mouse_x, mouse_y;
    glfwGetCursorPos(polled_window_handle, &mouse_x, &mouse_y);

    current_mouse_pos = glm::vec2(static_cast<f32>(mouse_x), static_cast<f32>(mouse_y));
    mouse_delta = current_mouse_pos - last_mouse_pos;
    last_mouse_pos = current_mouse_pos;

    std::copy(current_key_presses.begin(), current_key_presses.end(), last_key_presses.data());
    std::copy(current_button_presses.begin(), current_button_presses.end(), last_button_presses.data());

    for(i32 key = static_cast<i32>(Key::FIRST_KEY); key <= static_cast<i32>(Key::LAST_KEY); ++key) {
        current_key_presses[key - static_cast<i32>(Key::FIRST_KEY)] = (glfwGetKey(polled_window_handle, key) > 0);
    }
    for(i32 button = static_cast<i32>(Button::FIRST_BUTTON); button <= static_cast<i32>(Button::LAST_BUTTON); ++button) {
        current_button_presses[button - static_cast<i32>(Button::FIRST_BUTTON)] = (glfwGetMouseButton(polled_window_handle, button) > 0);
    }
}

void InputManager::set_cursor_mode(CursorMode mode) {
    cursor_mode = mode;

    glfwSetInputMode(polled_window_handle, GLFW_CURSOR, static_cast<i32>(mode));
}

bool InputManager::get_key(Key key, InputState state) const {
    u32 key_id = static_cast<u32>(key);
    constexpr u32 first_key = static_cast<u32>(Key::FIRST_KEY);

    switch (state) {
        case InputState::Pressed:
            return current_key_presses[key_id - first_key] && !last_key_presses[key_id - first_key];
        case InputState::Released:
            return !current_key_presses[key_id - first_key] && last_key_presses[key_id - first_key];
        case InputState::Down:
            return current_key_presses[key_id - first_key];
    }
}
bool InputManager::get_button(Button button, InputState state) const {
    u32 button_id = static_cast<u32>(button);
    constexpr u32 first_button = static_cast<u32>(Button::FIRST_BUTTON);

    switch (state) {
        case InputState::Pressed:
            return current_button_presses[button_id - first_button] && !last_button_presses[button_id - first_button];
        case InputState::Released:
            return !current_button_presses[button_id - first_button] && last_button_presses[button_id - first_button];
        case InputState::Down:
            return current_button_presses[button_id - first_button];
    }
}

void InputManager::scroll_callback(GLFWwindow *window, f64 x_offset, f64 y_offset) {
    alive_managers[window]->scroll_delta = glm::vec2(static_cast<f32>(x_offset), static_cast<f32>(y_offset));
}
void InputManager::key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods) {
    if(action == GLFW_PRESS) {
        alive_managers[window]->last_key = static_cast<Key>(key);
    }
}
void InputManager::mouse_button_callback(GLFWwindow *window, i32 button, i32 action, i32 mods) {
    if(action == GLFW_PRESS) {
        alive_managers[window]->last_button = static_cast<Button>(button);
    }
}