#include <window.hpp>

#include <GLFW/glfw3.h>
#include <debug.hpp>

static GLFWwindow* window_handle = nullptr;

void Window::open(const Config& config) {
    DEBUG_ASSERT(window_handle == nullptr)
    DEBUG_ASSERT(glfwInit())

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    if (config.fullscreen)
        window_handle = glfwCreateWindow(mode->width, mode->height, config.title.c_str(), monitor, nullptr);
    else {
        glfwWindowHint(GLFW_RESIZABLE, config.resizable);
        window_handle = glfwCreateWindow(static_cast<int>(config.width), static_cast<int>(config.height), config.title.c_str(), nullptr, nullptr);
    }

    DEBUG_ASSERT(window_handle != nullptr)
}

void Window::close() {
    DEBUG_ASSERT(window_handle != nullptr)

    glfwDestroyWindow(window_handle);
    glfwTerminate();
}

bool Window::should_close() {
    return glfwWindowShouldClose(window_handle);
}

void Window::poll_events() {
    glfwPollEvents();
}
