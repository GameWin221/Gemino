#include <window/window.hpp>
#include <renderer/renderer.hpp>
#include <world/world.hpp>

#include <common/types.hpp>
#include <common/utils.hpp>
#include <common/debug.hpp>
#include <render_paths/raster_render_path.hpp>

#include <random>
#include <thread>
#include <iomanip>

int main(){
    Window window(WindowConfig{
        .title = "Gemino Engine Example",
        .fullscreen = false,
        .resizable = true
    });

    RasterRenderPath render_path(window, VSyncMode::Disabled);

    auto texture = Utils::load_u8_image("res/texture.png", 4U);
    auto texture_monkey = Utils::load_u8_image("res/monkey.jpg", 4U);
    auto texture_handle = render_path.create_u8_texture(texture.pixels, texture.width, texture.height, texture.bytes_per_pixel, true, false);
    auto texture_monkey_handle = render_path.create_u8_texture(texture_monkey.pixels, texture_monkey.width, texture_monkey.height, texture_monkey.bytes_per_pixel, true, false);
    texture.free();
    texture_monkey.free();

    auto mesh = Utils::load_obj("res/monkey.obj");
    auto mesh_handle = render_path.create_mesh(mesh.sub_meshes[0].vertices, mesh.sub_meshes[0].indices);
    mesh.free();

    auto material_handle = render_path.create_material(
        texture_handle, INVALID_HANDLE, INVALID_HANDLE, INVALID_HANDLE, glm::vec3(1.0f, 1.0f, 1.0f)
    );
    auto material_monkey_handle = render_path.create_material(
        texture_monkey_handle, INVALID_HANDLE, INVALID_HANDLE, INVALID_HANDLE, glm::vec3(1.0f, 0.4f, 1.0f)
    );

    std::srand(std::time(nullptr));


    World world{};
    for(u32 x{}; x < 10U; ++x) {
        for(u32 y{}; y < 10U; ++y) {
            glm::mat4 mat(1.0f);

            mat = glm::translate(mat, glm::vec3(x*3U, 0.0f, y*3U));
            mat = glm::rotate(mat, glm::radians((std::rand() % 3600) / 10.0f), glm::vec3(0, 1, 0));
            mat = glm::rotate(mat, glm::radians((std::rand() % 3600) / 10.0f), glm::vec3(0, 0, 1));
            mat = glm::rotate(mat, glm::radians((std::rand() % 3600) / 10.0f), glm::vec3(1, 0, 0));

            world.create_object(mesh_handle, (((x + y) % 2 == 0) ? material_handle : material_monkey_handle), mat);
        }
    }

    auto main_camera = world.create_camera(glm::vec2(window.get_size()), glm::vec3(-2.0f, 10.0f, -2.0f), -25.0f);

    double dt = 1.0, time{};

    DEBUG_TIMESTAMP(last_frame);

    while(window.is_open()) {
        world.set_camera_rotation(main_camera, world.get_camera_pitch(main_camera),45.0f + glm::degrees(0.15f * glm::pi<f32>() * std::sin(static_cast<f32>(time / 2.0f))));
        //world.set_camera_rotation(main_camera, world.get_camera_pitch(main_camera),glm::degrees(-static_cast<f32>(time) - (glm::pi<float>() / 2.0f)));
        //world.set_camera_position(main_camera, glm::vec3(std::sin(static_cast<f32>(time)) * 4.0f, 10.0f,std::cos(static_cast<f32>(time)) * 4.0f));

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

        DEBUG_TIMESTAMP(now);
        dt = DEBUG_TIME_DIFF(last_frame, now);
        last_frame = now;

        DEBUG_LOG(1.0 / dt << "fps")

        time += dt;
    }
}