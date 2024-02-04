#include <window/window.hpp>
#include <window/input_manager.hpp>
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

    InputManager input(window);

    RasterRenderPath render_path(window, VSyncMode::Adaptive);

    auto texture = Utils::load_u8_image("res/texture.png", 4U);
    auto texture_monkey = Utils::load_u8_image("res/monkey.jpg", 4U);

    auto texture_handle = render_path.create_u8_texture(texture.pixels, texture.width, texture.height, texture.bytes_per_pixel, true, true);
    auto texture_monkey_handle = render_path.create_u8_texture(texture_monkey.pixels, texture_monkey.width, texture_monkey.height, texture_monkey.bytes_per_pixel, true, true);

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
            for(u32 z{}; z < 10U; ++z) {
                world.create_object(
                        mesh_handle,
                        (((x + y + z) % 2 == 0) ? material_handle : material_monkey_handle),
                        glm::vec3(x * 4U, z * 2U, y * 4U),
                        glm::vec3((std::rand() % 3600) / 10.0f, (std::rand() % 3600) / 10.0f, (std::rand() % 3600) / 10.0f
                    )
                );
            }
        }
    }

    auto main_camera = world.create_camera(glm::vec2(window.get_size()), glm::vec3(-2.0f, 10.0f, -2.0f), -25.0f, 0.0f, 60.0f, 0.02f, 40000.0f);

    double dt = 1.0, time{};

    DEBUG_TIMESTAMP(last_frame);

    input.set_cursor_mode(CursorMode::Locked);

    while(window.is_open()) {
        input.poll_input();

        const auto& main_camera_data = world.get_camera(main_camera);

        float camera_movement_speed = 6.0f;
        float camera_rotate_speed = 0.1f;

        glm::vec2 mouse_vel = input.get_mouse_velocity();
        glm::vec3 camera_movement{};
        if(input.get_key(Key::W, InputState::Down)) {
            camera_movement += main_camera_data.forward;
        } else if(input.get_key(Key::S, InputState::Down)) {
            camera_movement -= main_camera_data.forward;
        }

        if(input.get_key(Key::A, InputState::Down)) {
            camera_movement -= main_camera_data.right;
        } else if(input.get_key(Key::D, InputState::Down)) {
            camera_movement += main_camera_data.right;
        }

        if(input.get_key(Key::Space, InputState::Down)) {
            camera_movement += main_camera_data.up;
        } else if(input.get_key(Key::LeftControl, InputState::Down)) {
            camera_movement -= main_camera_data.up;
        }

        if(input.get_key(Key::LeftShift, InputState::Down)) {
            camera_movement_speed += 8.0f;
        }

        if(input.get_key(Key::Escape, InputState::Pressed)) {
            input.set_cursor_mode(CursorMode::Free);
        }
        if(input.get_button(Button::Left, InputState::Pressed)) {
            input.set_cursor_mode(CursorMode::Locked);
        }

        world.set_camera_position(main_camera, main_camera_data.position + camera_movement * static_cast<f32>(dt) * camera_movement_speed);
        world.set_camera_rotation(main_camera, main_camera_data.pitch - mouse_vel.y * camera_rotate_speed, main_camera_data.yaw + mouse_vel.x * camera_rotate_speed);

        //world.set_camera_position(main_camera, glm::vec3(2.0f, 1.0f, 2.0f) + glm::vec3(std::sin(static_cast<f32>(time)) * 5.0f, 0.0f, std::cos(static_cast<f32>(time)) * 5.0f));
        //world.set_camera_rotation(main_camera, 0.0f, -static_cast<f32>(time) / glm::pi<f32>() * 180.0f - 90.0f);

        //world.set_camera_position(main_camera, glm::vec3(2.0f + std::sinf(static_cast<f32>(time * 3.0f)) * 3.0f, 0.0f, -5.0f));
        //world.set_camera_rotation(main_camera, 0.0f, 90.0f);

        //for(Handle<Object> handle{}; handle < static_cast<u32>(world.get_objects().size()); ++handle) {
        //    Handle<Transform> t = world.get_object(handle).transform;
        //    world.set_rotation(handle, world.get_transform(t).rotation + glm::vec3(static_cast<f32>(dt) * 40.0f, static_cast<f32>(dt) * 20.0f, 0.0f));
        //}

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

        if(render_path.get_frames_since_init() % 128 == 0) {
            DEBUG_LOG(1.0 / dt << "fps")
        }

        time += dt;
    }
}