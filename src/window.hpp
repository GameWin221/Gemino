#ifndef GEMINO_WINDOW_HPP
#define GEMINO_WINDOW_HPP

#include <string>
#include <types.hpp>

struct WindowConfig {
    std::string title{};

    u32 width = 1920U;
    u32 height = 1080U;

    bool fullscreen = false;
    bool resizable = false;
};

class Window {
public:
    Window(const WindowConfig& config);

    void close();

    bool should_close();
    void poll_events();

    inline const Proxy get_native_handle() const { return reinterpret_cast<Proxy>(window_handle); }

private:
    struct GLFWwindow* window_handle{};
};

#endif
