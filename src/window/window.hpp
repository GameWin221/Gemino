#ifndef GEMINO_WINDOW_HPP
#define GEMINO_WINDOW_HPP

#include <string>
#include <functional>
#include <vector>
#include <common/types.hpp>
#include <glm/vec2.hpp>

struct WindowConfig {
    std::string title{"Gemino Engine"};

    glm::uvec2 windowed_size{1920U, 1080U};

    bool fullscreen = false;
    bool resizable = false;
};

class Window {
public:
    Window(const WindowConfig& config);
    ~Window();

    Window& operator=(const Window& other) = delete;
    Window& operator=(Window&& other) noexcept = delete;

    void set_title(const std::string& title);
    void set_size(glm::uvec2 windowed_size);
    void set_fullscreen(bool fullscreen);

    void force_close();
    void poll_events();

    bool was_resized_last_time();

    bool is_open() const;
    bool is_window_size_nonzero() const;

    glm::uvec2 get_size() const;

    Proxy get_native_handle() const { return static_cast<Proxy>(window_handle); }

    const WindowConfig& get_config() const { return window_config; }

    void add_on_resize_callback(std::function<void(u32, u32)> fn);

private:
    WindowConfig window_config{};

    struct GLFWwindow* window_handle{};

    static void framebuffer_size_callback(struct GLFWwindow* window, i32 width, i32 height);

    bool was_resized = false;
};

#endif
