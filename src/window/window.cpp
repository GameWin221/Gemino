#include "window.hpp"
#include <common/debug.hpp>

#include <unordered_map>
#include <GLFW/glfw3.h>

static std::unordered_map<GLFWwindow*, Window*> alive_windows{};

Window::Window(const WindowConfig &config) : m_window_config(config) {
    DEBUG_ASSERT(glfwInit())

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    if (config.fullscreen) {
        m_window_handle = glfwCreateWindow(mode->width, mode->height, config.title.c_str(), monitor, nullptr);
    } else {
        glfwWindowHint(GLFW_RESIZABLE, config.resizable);
        m_window_handle = glfwCreateWindow(static_cast<int>(config.windowed_size.x), static_cast<int>(config.windowed_size.y), config.title.c_str(), nullptr, nullptr);
    }

    DEBUG_ASSERT(m_window_handle != nullptr)

    alive_windows[m_window_handle] = this;

    glfwSetFramebufferSizeCallback(m_window_handle, Window::framebuffer_size_callback);
}

Window::~Window() {
    DEBUG_ASSERT(m_window_handle != nullptr)

    alive_windows.erase(m_window_handle);

    glfwDestroyWindow(m_window_handle);
    glfwTerminate();
}

void Window::set_title(const std::string &title) {
    m_window_config.title = title;
    glfwSetWindowTitle(m_window_handle, title.c_str());
}
void Window::set_size(glm::uvec2 windowed_size) {
    m_window_config.windowed_size = windowed_size;
    glfwSetWindowSize(m_window_handle, static_cast<i32>(windowed_size.x), static_cast<i32>(windowed_size.y));
}
void Window::set_fullscreen(bool fullscreen) {
    m_window_config.fullscreen = fullscreen;

    if(fullscreen) {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        glfwSetWindowMonitor(m_window_handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        glfwSetWindowMonitor(m_window_handle, nullptr, 0, 30, static_cast<i32>(m_window_config.windowed_size.x), static_cast<i32>(m_window_config.windowed_size.y), 0);
    }
}

bool Window::is_open() const {
    return !glfwWindowShouldClose(m_window_handle);
}
bool Window::is_window_size_nonzero() const {
    glm::uvec2 size = get_size();
    return size.x != 0U && size.y != 0U;
}

void Window::poll_events() {
    glfwPollEvents();
}

glm::uvec2 Window::get_size() const {
    int w, h;
    glfwGetFramebufferSize(m_window_handle, &w, &h);

    return glm::uvec2{ static_cast<u32>(w), static_cast<u32>(h) };
}

void Window::force_close() {
    glfwSetWindowShouldClose(m_window_handle, true);
}

void Window::framebuffer_size_callback(struct GLFWwindow *window, i32 width, i32 height) {
    alive_windows[window]->was_resized = true;
}

bool Window::was_resized_last_time() {
    bool r = was_resized;
    was_resized = false;
    return r;
}
