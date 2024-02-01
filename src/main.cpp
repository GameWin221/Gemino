#include <window/window.hpp>
#include <renderer/renderer.hpp>
#include <world/world.hpp>
#include <thread>
#include <common/types.hpp>
#include <common/utils.hpp>
#include <common/debug.hpp>
#include <render_paths/raster_render_path.hpp>

int main(){
    Window window(WindowConfig{
        .title = "Gemino Engine Example",
        .fullscreen = false,
        .resizable = true
    });

    RasterRenderPath render_path(window, VSyncMode::Enabled);

    auto mesh = Utils::import_obj_mesh("res/monkey.obj")[0];
    auto mesh_handle = render_path.create_mesh(mesh.vertices, mesh.indices);

    World world{};
    auto object_monkey = world.create_object(
        mesh_handle, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f))
    );

    auto main_camera = world.create_camera(glm::vec2(window.get_size()), glm::vec3(0.0f), -15.0f, 0.0f, 70.0f);

    double dt = 1.0, time{};

    while(window.is_open()) {
        DEBUG_TIMESTAMP(start);
        world.set_camera_rotation(main_camera, world.get_camera_pitch(main_camera),glm::degrees(-static_cast<f32>(time) - (glm::pi<float>() / 2.0f)));
        world.set_camera_position(main_camera, glm::vec3(std::sin(static_cast<f32>(time)) * 4.0f, 2.0f,std::cos(static_cast<f32>(time)) * 4.0f));

        if(window.is_window_size_nonzero()) {
            if(window.was_resized_last_time()) {
                glm::uvec2 window_size = window.get_size();

                world.set_camera_viewport(main_camera, window_size);

                DEBUG_LOG("Resized to: " << window_size.x << " x " << window_size.y)

                render_path.resize(window);
            } else {
                render_path.render(world, main_camera);
                world.render_finished_tick();
            }
        }

        window.poll_events();

        DEBUG_TIMESTAMP(end);
        dt = DEBUG_TIME_DIFF(start, end);

        time += 0.01;//dt;
    }
}