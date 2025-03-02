#include <window/window.hpp>
#include <window/input_manager.hpp>
#include <world/world.hpp>
#include <editor/editor.hpp>
#include <renderer/renderer.hpp>

#include <common/types.hpp>
#include <common/utils.hpp>
#include <common/debug.hpp>

#include <random>
#include <thread>
#include <iomanip>

//constexpr const char *SPONZA_PATH = "C:/Dev/Resources/Meshes/main1_sponza/NewSponza_Main_glTF_003.gltf";
constexpr const char *BISTRO_PATH = "C:/Dev/Resources/Meshes/Bistro_v5_2/GLTF/BistroExterior.gltf";

int main(){
    Window window(WindowConfig {
        .title = "Gemino Engine Example",
        .windowed_size = glm::uvec2(1920u, 1080u),
        .fullscreen = false,
        .resizable = true
    });

    InputManager input(window);

    Renderer renderer(window, VSyncMode::Enabled);

    Editor::attach(renderer);

    World world{};

    auto bistro_scene = renderer.load_gltf_scene(SceneLoadInfo {
        .path = BISTRO_PATH,
        .import_textures = false,
        .import_materials = true,
        .lod_bias_vert_threshold = 10000u,
        .lod_bias = 0.8f
    });

    auto bistro_handle = world.instantiate_scene(bistro_scene);

    auto monkey_scene = renderer.load_gltf_scene(SceneLoadInfo {
        .path = "res/monkey.gltf"
    });

    monkey_scene.position = glm::vec3(4.0f, 0.5f, 0.0f);
    monkey_scene.rotation = glm::angleAxis(glm::radians(-45.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(glm::radians(-35.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    auto monkey_handle = world.instantiate_scene_object(monkey_scene, 0);

    for (u32 y{}; y < 10u; ++y) {
        for (u32 x{}; x < 10u; ++x) {
            for (u32 z{}; z < 10u; ++z) {
                monkey_scene.position = glm::vec3(x * 2u, y * 2u, z * 2u);
                //auto monkey_handle = world.instantiate_scene_object(monkey_scene, 0u);
            }
        }
    }

    std::srand(0xDEADBEEF);

    auto main_camera = world.create_camera(CameraCreateInfo{
        .viewport_size = glm::vec2(window.get_size()),
        .position = glm::vec3(-9.98111f, 1.07925f, 2.21961f),
        .pitch = 30.7f,
        .yaw = 30.6f,
        .fov = 60.0f,
        .near_plane = 0.01f,
        .far_plane = 2000.0f
    });

    double dt = 1.0, time{};

    DEBUG_TIMESTAMP(last_frame);

    while(window.is_open()) {
        input.poll_input();

        const auto &main_camera_data = world.get_camera(main_camera);

        float camera_movement_speed = 6.0f;
        float camera_rotate_speed = 0.1f;

        glm::vec2 mouse_vel{};
        if(input.get_button(Button::Right, InputState::Down)) {
           mouse_vel = input.get_mouse_velocity();
        }

        if(input.get_button(Button::Right, InputState::Pressed)) {
            input.set_cursor_mode(CursorMode::Locked);
        } else if(input.get_button(Button::Right, InputState::Released)) {
            input.set_cursor_mode(CursorMode::Free);
        }

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

        world.set_camera_position(main_camera, main_camera_data.position + camera_movement * static_cast<f32>(dt) * camera_movement_speed);
        world.set_camera_rotation(main_camera, main_camera_data.pitch - mouse_vel.y * camera_rotate_speed, main_camera_data.yaw + mouse_vel.x * camera_rotate_speed);

        f32 f_time = static_cast<f32>(time);
        f32 f_dt = static_cast<f32>(dt);

        if(input.get_key(Key::LeftShift, InputState::Down)) {
            if(input.get_key(Key::U, InputState::Pressed)) {
                if (Editor::is_attached()) {
                    Editor::detach(renderer);
                } else {
                    Editor::attach(renderer);
                }
            }

            if(input.get_key(Key::R, InputState::Pressed)) {
                renderer.set_config_enable_dynamic_lod(!renderer.get_shared_objects().config_enable_dynamic_lod);
                DEBUG_LOG("dynamic_lod " << (renderer.get_shared_objects().config_enable_dynamic_lod ? "enabled" : "disabled"))
            } else if(input.get_key(Key::T, InputState::Pressed)) {
                renderer.set_config_enable_frustum_cull(!renderer.get_shared_objects().config_enable_frustum_cull);
                DEBUG_LOG("frustum_cull " << (renderer.get_shared_objects().config_enable_frustum_cull ? "enabled" : "disabled"))
            } else if(input.get_key(Key::B, InputState::Pressed)) {
                renderer.set_config_enable_debug_shape_view(!renderer.get_shared_objects().config_enable_debug_shape_view);
                DEBUG_LOG("debug_shape_view " << (renderer.get_shared_objects().config_enable_debug_shape_view ? "enabled" : "disabled"))
            } else if(input.get_key(Key::Q, InputState::Pressed)) {
                renderer.reload_pipelines();
            }

            if(input.get_key(Key::G, InputState::Pressed)) {
                DEBUG_LOG("Position: " << main_camera_data.position.x << "f, " << main_camera_data.position.y << "f, " << main_camera_data.position.z << "f")
                DEBUG_LOG("Rotation: " << main_camera_data.pitch << "f, " << main_camera_data.yaw << "f")
            }
        }

        world.update_objects();

        if(window.is_window_size_nonzero()) {
            if(window.was_resized_last_time()) {
                glm::uvec2 window_size = window.get_size();

                world.set_camera_viewport(main_camera, window_size);

                DEBUG_LOG("Resized to: " << window_size.x << " x " << window_size.y)

                renderer.resize(window);
            } else {
                renderer.render(window, world, main_camera);
            }
        }


        window.poll_events();

        DEBUG_TIMESTAMP(now);
        dt = DEBUG_TIME_DIFF(last_frame, now);
        _DEBUG_TIMESTAMP_NAME(last_frame) = _DEBUG_TIMESTAMP_NAME(now);

        if(renderer.get_shared_objects().frames_since_init % 512u == 0u) {
            DEBUG_LOG(1.0 / dt << "fps")
        }

        time += dt;
    }
}
