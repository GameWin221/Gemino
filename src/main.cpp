#include <window/window.hpp>
#include <window/input_manager.hpp>
#include <world/world.hpp>

#include <common/types.hpp>
#include <common/utils.hpp>
#include <common/debug.hpp>
#include <renderer/renderer.hpp>

#include <random>
#include <thread>
#include <iomanip>

int main(){
    Window window(WindowConfig {
        .title = "Gemino Engine Example",
        .windowed_size = glm::uvec2(1920u, 1080u),
        .fullscreen = false,
        .resizable = true
    });

    InputManager input(window);

    Renderer render_path(window, VSyncMode::Adaptive);

    World world{};

    auto sponza_scene = render_path.load_gltf_scene(SceneLoadInfo {
       .path = "res/Sponza/Main/SponzaMain.gltf",
       .import_textures = false,
       .import_materials = true
    });

    sponza_scene.rotation = glm::angleAxis(glm::radians(-45.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    auto sponza_handles = world.instantiate_scene(sponza_scene);

    auto monkey_scene = render_path.load_gltf_scene(SceneLoadInfo {
        .path = "res/monkey.gltf"
    });

    monkey_scene.position = glm::vec3(4.0f, 0.5f, 0.0f);
    monkey_scene.rotation = glm::angleAxis(glm::radians(-45.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(glm::radians(-35.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    world.instantiate_scene_object(monkey_scene, 0);

    std::srand(0xDEADBEEF);

    /*
    for(u32 x{}; x < 10u; ++x) {
        for(u32 y{}; y < 10u; ++y) {
            for(u32 z{}; z < 10u; ++z) {
                glm::quat rotation_x = glm::angleAxis(glm::radians((std::rand() % 3600) / 10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                glm::quat rotation_y = glm::angleAxis(glm::radians((std::rand() % 3600) / 10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                glm::quat rotation_z = glm::angleAxis(glm::radians((std::rand() % 3600) / 10.0f), glm::vec3(0.0f, 0.0f, 1.0f));

                world.create_object(ObjectCreateInfo{
                    //.mesh = ((x + y + z) % 2 == 0) ? monkey_mesh_handle : sphere_mesh_handle,
                    //.material = (((x + y + z) % 2 == 0) ? material_monkey_handle : material_handle),
                    //.mesh = monkey_mesh_handle,
                    //.material = material_monkey_handle,
                    .position = glm::vec3(x * 3U, y * 2U, z * 3U),
                    .rotation = rotation_x * rotation_y * rotation_z
                });
            }
        }
    }
*/

    auto main_camera = world.create_camera(CameraCreateInfo{
        .viewport_size = glm::vec2(window.get_size()),
        .position = glm::vec3(-9.98111f, 1.07925f, 2.21961f),
        .pitch = -5.7f,
        .yaw = -17.6f,
        .fov = 60.0f,
        .near_plane = 0.01f,
        .far_plane = 2000.0f
    });

    double dt = 1.0, time{};

    DEBUG_TIMESTAMP(last_frame);

    input.set_cursor_mode(CursorMode::Locked);

    while(window.is_open()) {
        input.poll_input();

        const auto &main_camera_data = world.get_camera(main_camera);

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

        f32 f_time = static_cast<f32>(time);

        //world.set_camera_position(main_camera, glm::vec3(40.4874f, 7.70619f, 29.7653f));
        //world.set_camera_rotation(main_camera,glm::mix(19.1001f, 20.1001f, sinf(f_time)),glm::mix(-84.5997f, -85.5997f, sinf(f_time)));

        if(input.get_key(Key::R, InputState::Pressed)) {
            render_path.set_config_enable_dynamic_lod(!render_path.get_config_enable_dynamic_lod());
            DEBUG_LOG("dynamic_lod " << (render_path.get_config_enable_dynamic_lod() ? "enabled" : "disabled"))
        } else if(input.get_key(Key::T, InputState::Pressed)) {
            render_path.set_config_enable_frustum_cull(!render_path.get_config_enable_frustum_cull());
            DEBUG_LOG("frustum_cull " << (render_path.get_config_enable_frustum_cull() ? "enabled" : "disabled"))
        }

        if(input.get_key(Key::G, InputState::Pressed)) {
            DEBUG_LOG("Position: " << main_camera_data.position.x << "f, " << main_camera_data.position.y << "f, " << main_camera_data.position.z << "f")
            DEBUG_LOG("Rotation: " << main_camera_data.pitch << "f, " << main_camera_data.yaw << "f")
        }

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
            }
        }

        window.poll_events();

        DEBUG_TIMESTAMP(now);
        dt = DEBUG_TIME_DIFF(last_frame, now);
        last_frame = now;

        if(render_path.get_frames_since_init() % 512u == 0u) {
            DEBUG_LOG(1.0 / dt << "fps")
        }

        time += dt;
    }
}