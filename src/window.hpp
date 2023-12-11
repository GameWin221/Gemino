#ifndef GEMINI_WINDOW_HPP
#define GEMINI_WINDOW_HPP

#include <string>
#include <types.hpp>

namespace Window {
    struct Config {
        std::string title{};

        u32 width = 1920U;
        u32 height = 1080U;

        bool fullscreen = false;
        bool resizable = false;
    };

    void open(const Config& config);
    void close();

    bool should_close();
    void poll_events();
};

#endif
