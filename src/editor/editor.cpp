#include "editor.hpp"

#include <algorithm>
#include <imgui.h>
#include <implot.h>
#include <ImGuiProfilerRenderer.h>

static bool g_is_attached = false;
static bool g_gpu_memory_usage_window_open = true;
static bool g_frametime_window_open = true;

struct GPUMemoryElement {
    std::string format{};
    usize bytes{};
    usize capacity{};
};

void Editor::attach(Renderer &renderer) {
    renderer.m_ui_pass_draw_fn = Editor::draw;
    g_is_attached = true;
}
void Editor::detach(Renderer &renderer) {
    renderer.m_ui_pass_draw_fn = nullptr;
    g_is_attached = false;
}

bool Editor::is_attached() {
    return g_is_attached;
}

void Editor::draw(Renderer &renderer, World &world) {
    if (g_gpu_memory_usage_window_open) {
        draw_gpu_memory_usage_window(renderer, world);
    }
    if (g_frametime_window_open) {
        draw_frametime_window(renderer);
    }
}

void Editor::draw_gpu_memory_usage_window(Renderer &renderer, World &world) {
    if(!ImGui::Begin("Memory Usage", &g_gpu_memory_usage_window_open)) {
        ImGui::End();
        return;
    }

    static char buf[128];

    ImGui::SeparatorText("Pre-allocated GPU buffer sizes:");

    std::vector<GPUMemoryElement> pre_allocated_gpu_memory_elements{
        {"Bindless Texture Descriptor       : %.02f mb", renderer.MAX_SCENE_TEXTURES * 96},     // https://gist.github.com/nanokatze/bb03a486571e13a7b6a8709368bd87cf#file-04-descriptors-md
        {"Scene Material Buffer             : %.02f mb", renderer.MAX_SCENE_MATERIALS * sizeof(Material)},
        {"Scene Vertex Buffer               : %.02f mb", renderer.MAX_SCENE_VERTICES * sizeof(Vertex)},
        {"Scene Index Buffer                : %.02f mb", renderer.MAX_SCENE_INDICES * sizeof(u32)},
        {"Scene Mesh Buffer                 : %.02f mb", renderer.MAX_SCENE_MESHES * sizeof(Mesh)},
        {"Scene MeshInstance Buffer         : %.02f mb", renderer.MAX_SCENE_MESH_INSTANCES * sizeof(MeshInstance)},
        {"Scene MeshInstance Material Buffer: %.02f mb", renderer.MAX_SCENE_MESH_INSTANCE_MATERIALS * sizeof(Handle<Material>)},
        {"Scene Primitive Buffer            : %.02f mb", renderer.MAX_SCENE_PRIMITIVES * sizeof(Primitive)},
        {"Scene Object Buffer               : %.02f mb", renderer.MAX_SCENE_OBJECTS * sizeof(Object)},
        {"Scene Global Transform Buffer     : %.02f mb", renderer.MAX_SCENE_OBJECTS * sizeof(Transform)},
        {"Scene Draw Buffer                 : %.02f mb", renderer.MAX_SCENE_DRAWS * sizeof(DrawCommand)},
    };

    usize pre_allocated_total_size{};
    for (const auto &[format, bytes, capacity] : pre_allocated_gpu_memory_elements) {
        sprintf(buf, format.c_str(), static_cast<f32>(bytes) / 1024.0f / 1024.0f);
        ImGui::Text(buf);

        pre_allocated_total_size += bytes;
    }

    ImGui::Spacing();
    ImGui::Text("Total: %.02f mb", static_cast<f32>(pre_allocated_total_size) / 1024.0f / 1024.0f);

    ImGui::SeparatorText("Used GPU memory");

    std::vector<GPUMemoryElement> used_gpu_memory_elements{
        {"Allocated Textures              : %.04f / %.04f mb", 0, 0},
        {"Allocated Materials             : %.04f / %.04f mb", renderer.get_material_allocator().get_valid_handles().size() * sizeof(Material), renderer.MAX_SCENE_MATERIALS * sizeof(Material)},
        {"Allocated Vertices              : %.04f / %.04f mb", 0, renderer.MAX_SCENE_VERTICES * sizeof(Vertex)},
        {"Allocated Indices               : %.04f / %.04f mb", 0, renderer.MAX_SCENE_INDICES * sizeof(u32) },
        {"Allocated Meshes                : %.04f / %.04f mb", renderer.get_mesh_allocator().get_valid_handles().size() * sizeof(Mesh), renderer.MAX_SCENE_MESHES * sizeof(Mesh) },
        {"Allocated MeshInstances         : %.04f / %.04f mb", renderer.get_mesh_instance_allocator().get_valid_handles().size() * sizeof(MeshInstance), renderer.MAX_SCENE_MESH_INSTANCES * sizeof(MeshInstance) },
        {"Allocated MeshInstance Materials: %.04f / %.04f mb", 0, renderer.MAX_SCENE_MESH_INSTANCE_MATERIALS * sizeof(Handle<Material>) },
        {"Allocated Primitives            : %.04f / %.04f mb", 0, renderer.MAX_SCENE_PRIMITIVES * sizeof(Primitive) },
        {"Allocated Objects               : %.04f / %.04f mb", world.get_valid_object_handles().size() * sizeof(Object), renderer.MAX_SCENE_OBJECTS * sizeof(Object) },
        {"Allocated Global Transforms     : %.04f / %.04f mb", world.get_valid_object_handles().size() * sizeof(Transform), renderer.MAX_SCENE_DRAWS * sizeof(DrawCommand) },
    };

    for(const auto &elem : renderer.get_texture_allocator().get_valid_handles()) {
        const Texture &texture = renderer.get_texture_allocator().get_element(elem);

        u32 w = texture.width, h = texture.height;
        for(u32 i{}; i < texture.mip_level_count; ++i) {
            used_gpu_memory_elements[0].bytes += w * h * texture.bytes_per_pixel;

            w = std::max(1u, w / 2u);
            h = std::max(1u, h / 2u);
        }
    }

    for(const auto &[start, count] : renderer.get_vertex_allocator().get_valid_ranges()) {
        used_gpu_memory_elements[2].bytes += count * sizeof(Vertex);
    }
    for(const auto &[start, count] : renderer.get_index_allocator().get_valid_ranges()) {
        used_gpu_memory_elements[3].bytes += count * sizeof(u32);
    }
    for(const auto &[start, count] : renderer.get_mesh_instance_materials_allocator().get_valid_ranges()) {
        used_gpu_memory_elements[6].bytes += count * sizeof(Handle<Material>);
    }
    for(const auto &[start, count] : renderer.get_primitive_allocator().get_valid_ranges()) {
        used_gpu_memory_elements[7].bytes += count * sizeof(Primitive);
    }

    usize used_total_size{};
    for (const auto &[format, bytes, capacity] : used_gpu_memory_elements) {
        sprintf(buf, format.c_str(), static_cast<f32>(bytes) / 1024.0f / 1024.0f, static_cast<f32>(capacity) / 1024.0f / 1024.0f);
        ImGui::Text(buf);

        used_total_size += bytes;
    }

    ImGui::Spacing();
    ImGui::Text("Total: %.02f mb", static_cast<f32>(used_total_size) / 1024.0f / 1024.0f);

    ImGui::End();
}

void Editor::draw_frametime_window(Renderer &renderer) {
    if(!ImGui::Begin("Frametime", &g_frametime_window_open)) {
        ImGui::End();
        return;
    }

    static char buf[128];

    if(ImGui::CollapsingHeader("GPU Statistics")) {
        for(const auto &[name, data] : renderer.get_gpu_statistics()) {
            const auto &[queries, results] = data;

            sprintf(buf, "%s:", name.c_str()); ImGui::SeparatorText(buf);
            sprintf(buf, "\tinput_assembly_vertices: %u", results.input_assembly_vertices); ImGui::Text(buf);
            sprintf(buf, "\tinput_assembly_primitives: %u", results.input_assembly_primitives); ImGui::Text(buf);
            sprintf(buf, "\tvertex_shader_invocations: %u", results.vertex_shader_invocations); ImGui::Text(buf);
            sprintf(buf, "\tgeometry_shader_invocations: %u", results.geometry_shader_invocations); ImGui::Text(buf);
            sprintf(buf, "\tgeometry_shader_primitives: %u", results.geometry_shader_primitives); ImGui::Text(buf);
            sprintf(buf, "\tclipping_invocations: %u", results.clipping_invocations); ImGui::Text(buf);
            sprintf(buf, "\tclipping_primitives: %u", results.clipping_primitives); ImGui::Text(buf);
            sprintf(buf, "\tfragment_shader_invocations: %u", results.fragment_shader_invocations); ImGui::Text(buf);
            sprintf(buf, "\ttessellation_control_shader_patches: %u", results.tessellation_control_shader_patches); ImGui::Text(buf);
            sprintf(buf, "\ttessellation_evaluation_shader_invocations: %u", results.tessellation_evaluation_shader_invocations); ImGui::Text(buf);
            sprintf(buf, "\tcompute_shader_invocations: %u", results.compute_shader_invocations); ImGui::Text(buf);
            ImGui::Spacing();
        }

    }

    if(ImGui::CollapsingHeader("GPU Timing")) {
        static bool pause_gpu_profiler{};
        static u32 gpu_profile_frame_count = 100u;
        static ImGuiUtils::ProfilerGraph gpu_profiler_graph(gpu_profile_frame_count);
        gpu_profiler_graph.frameWidth = 4;
        gpu_profiler_graph.frameSpacing = 1;

        static constexpr std::array TASK_COLORS {
            legit::Colors::turqoise, legit::Colors::greenSea, legit::Colors::emerald, legit::Colors::nephritis,
            legit::Colors::peterRiver, legit::Colors::belizeHole, legit::Colors::amethyst, legit::Colors::wisteria,
            legit::Colors::sunFlower, legit::Colors::orange, legit::Colors::carrot, legit::Colors::pumpkin,
            legit::Colors::alizarin, legit::Colors::pomegranate, legit::Colors::clouds, legit::Colors::silver,
            legit::Colors::imguiText,
        };

        std::vector<legit::ProfilerTask> tasks{};

        f64 full_frame_start = renderer.get_gpu_timing().at("Total GPU Time").second.first;
        f64 full_frame_end = renderer.get_gpu_timing().at("Total GPU Time").second.second;
        f64 full_frame = full_frame_end - full_frame_start;

        sprintf(buf, "%s: %fms", "Total GPU Time", static_cast<f32>(full_frame)); ImGui::Text(buf);

        u32 color_id{};
        for(const auto &[name, data] : renderer.get_gpu_timing()) {
            const auto &[queries, time_pair] = data;

            if(name != "Total GPU Time") {
                tasks.push_back(legit::ProfilerTask{
                    .startTime = time_pair.first - full_frame_start,
                    .endTime = time_pair.second - full_frame_start,
                    .name = name,
                    .color = TASK_COLORS[(color_id++ * 3) % TASK_COLORS.size()],
                });
            }
        }

        static f32 avg_full_frame = full_frame;

        if(!pause_gpu_profiler) {
            std::sort(tasks.begin(), tasks.end(), [](const legit::ProfilerTask &a, const legit::ProfilerTask &b) {
                return a.startTime < b.startTime;
            });

            avg_full_frame = (avg_full_frame * 20.0f + full_frame) / 21.0f;
            gpu_profiler_graph.maxFrameTime = avg_full_frame * 1.1f;
            gpu_profiler_graph.LoadFrameData(tasks.data(), static_cast<u32>(tasks.size()));
        }
        gpu_profiler_graph.RenderTimings(
        std::clamp(
                static_cast<i32>(ImGui::GetWindowSize().x - 300),
                50,
                (gpu_profiler_graph.frameWidth + gpu_profiler_graph.frameSpacing) * static_cast<i32>(gpu_profile_frame_count)
            ),
            300,
            400,
            0
        );

        ImGui::Checkbox("Pause", &pause_gpu_profiler);
    }

    if(ImGui::CollapsingHeader("CPU Timing")) {
        for(const auto &[name, time] : renderer.get_cpu_timing()) {
            sprintf(buf, "%s: %fms", name.c_str(), static_cast<f32>(time * 1000.0));
            ImGui::Text(buf);
        }
    }

    ImGui::End();
}
