#include <iostream>

#include <window.hpp>
#include <instance.hpp>

#include <types.hpp>
#include <debug.hpp>

int main() {
    Instance::init();

    Window::open(Window::Config {
        .title = "Gemino Engine Example"
    });

    while(!Window::should_close()) {

        Window::poll_events();
    }

    Instance::deinit();

    Window::close();
}