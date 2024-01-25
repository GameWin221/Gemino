#include <window/window.hpp>
#include <renderer/renderer.hpp>
#include <world/world.hpp>

#include <common/types.hpp>
#include <common/debug.hpp>

//#include <tests.hpp>
#include <render_paths/raster_render_path.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main(){
    Window window(WindowConfig{
        .title = "Gemino Engine Example",
        .resizable = false
    });

    World world{};
    auto object_a = world.create_object(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f))
    );
    auto object_b = world.create_object(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))
    );
    auto object_c = world.create_object(
            glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.5f)), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
    );
    auto object_d = world.create_object(
            glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, -0.5f)), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
    );
    auto object_e = world.create_object(
            glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.0f)), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f))
    );
    auto object_f = world.create_object(
            glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.5f, 0.0f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f))
    );

    auto main_camera = world.create_camera(glm::vec2(window.get_size()), glm::vec3(0.0f), -15.0f, 0.0f, 70.0f);

    RasterRenderPath render_path(window, RendererConfig{
       .v_sync = VSyncMode::Enabled
    });

    float t{};
    while(!window.should_close()) {
        world.set_camera_position(main_camera, glm::vec3(std::sin(t) * 4.0f, 2.0f, std::cos(t) * 4.0f));
        world.set_camera_rotation(main_camera, world.get_camera_pitch(main_camera), glm::degrees(-t - (glm::pi<float>() / 2.0f)));
        //world.set_transform(test_object, glm::translate(glm::rotate(world.get_transform(test_object), 0.01f, glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(0.001f, 0.0f, 0.0f)));
        world.update_tick();

        render_path.begin_recording_frame();
        render_path.update_world(world, main_camera);
        render_path.render_world(world, main_camera);
        render_path.end_recording_frame();

        world.post_render_tick();

        window.poll_events();

        t += 0.005f;
    }

    //tests::run_api_test();
}