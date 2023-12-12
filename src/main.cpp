#include <iostream>

#include <window.hpp>
#include <instance.hpp>

#include <types.hpp>
#include <debug.hpp>

int main() {
    Window window(WindowConfig {
        .title = "Gemino Engine Example"
    });

    Instance instance(window.get_native_handle());

    while(!window.should_close()) {

        window.poll_events();
    }

    instance.destroy();
    window.close();
}