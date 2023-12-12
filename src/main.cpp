#include <iostream>

#include <window.hpp>
#include <swapchain.hpp>
#include <instance.hpp>

#include <types.hpp>
#include <debug.hpp>

int main() {
    Window window(WindowConfig {
        .title = "Gemino Engine Example"
    });

    Instance instance(window.get_native_handle());

    Swapchain swapchain(
        instance.get_device(),
        instance.get_physical_device(),
        instance.get_surface(),
        VkExtent2D{window.get_width(),window.get_height()},
        VSyncMode::Enabled
    );

    while(!window.should_close()) {

        window.poll_events();
    }

    swapchain.destroy(instance.get_device());
    instance.destroy();
    window.close();
}