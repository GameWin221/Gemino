#ifndef GEMINO_WINDOW_HPP
#define GEMINO_WINDOW_HPP

#include <string>
#include <common/types.hpp>
#include <glm/vec2.hpp>

struct WindowConfig {
    std::string title{"Gemino Engine"};

    glm::uvec2 size{1920U, 1080U};

    bool fullscreen = false;
    bool resizable = false;
};

class Window {
public:
    Window(const WindowConfig& config);
    ~Window();

    Window& operator=(const Window& other) = delete;
    Window& operator=(Window&& other) noexcept = delete;

    void force_close();

    bool should_close();
    void poll_events();

    glm::uvec2 get_size() const;

    Proxy get_native_handle() const { return static_cast<Proxy>(window_handle); }

private:
    struct GLFWwindow* window_handle{};
};

#endif
