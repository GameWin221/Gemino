#include "ui_pass.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void UIPass::init(const RenderAPI &api, const Window &window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    ImGui_ImplGlfw_InitForVulkan(reinterpret_cast<GLFWwindow *>(window.get_native_handle()), true);

    VkFormat swapchain_format = api.m_swapchain->get_format();
    m_extent = VkExtent2D{ window.get_size().x, window.get_size().y };

    VkAttachmentDescription attachment_description {
        .format = swapchain_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference attachment_reference {
        .attachment = 0u,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass_description {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1u,
        .pColorAttachments = &attachment_reference,
    };

    VkSubpassDependency subpass_dependency {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0u,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo render_pass_create_info {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1u,
        .pAttachments = &attachment_description,
        .subpassCount = 1u,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1u,
        .pDependencies = &subpass_dependency,
    };

    DEBUG_ASSERT(vkCreateRenderPass(api.m_instance->get_device(), &render_pass_create_info, nullptr, &m_render_pass) == VK_SUCCESS);

    m_framebuffers.resize(api.get_swapchain_image_count());
    for(u32 i{}; i < api.get_swapchain_image_count(); ++i) {
        const Image &image_data = api.m_resource_manager->get_image_data(api.get_swapchain_image_handle(i));

        VkFramebufferCreateInfo framebuffer_create_info {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = m_render_pass,
            .attachmentCount = 1u,
            .pAttachments = &image_data.view,
            .width = m_extent.width,
            .height = m_extent.height,
            .layers = 1u
        };

        DEBUG_ASSERT(vkCreateFramebuffer(api.m_instance->get_device(), &framebuffer_create_info, nullptr, &m_framebuffers[i]) == VK_SUCCESS);
    }

    ImGui_ImplVulkan_InitInfo init_info{
        .Instance = api.m_instance->get_instance(),
        .PhysicalDevice = api.m_instance->get_physical_device(),
        .Device = api.m_instance->get_device(),
        .QueueFamily = api.m_instance->get_queue_family_indices().graphics.value(),
        .Queue = api.m_instance->get_graphics_queue(),
        .DescriptorPool = api.m_resource_manager->get_descriptor_pool(),
        .RenderPass = m_render_pass,
        .MinImageCount = api.get_swapchain_image_count(),
        .ImageCount = api.get_swapchain_image_count(),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .CheckVkResultFn = [](VkResult result) {
            if (result != VK_SUCCESS) {
                DEBUG_PANIC("ImGui Vulkan Error!");
            }
        },
    };

    ImGui_ImplVulkan_Init(&init_info);
    ImGui_ImplVulkan_CreateFontsTexture();
}
void UIPass::destroy(const RenderAPI &api) {
    ImGui_ImplVulkan_Shutdown();

    for(const auto &framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(api.m_instance->get_device(), framebuffer, nullptr);
    }
    m_framebuffers.clear();

    vkDestroyRenderPass(api.m_instance->get_device(), m_render_pass, nullptr);

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIPass::resize(const RenderAPI &api, const Window &window) {
    m_extent = VkExtent2D{ window.get_size().x, window.get_size().y };

    for(const auto &framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(api.m_instance->get_device(), framebuffer, nullptr);
    }
    m_framebuffers.clear();

    m_framebuffers.resize(api.get_swapchain_image_count());
    for(u32 i{}; i < api.get_swapchain_image_count(); ++i) {
        const Image &image_data = api.m_resource_manager->get_image_data(api.get_swapchain_image_handle(i));

        VkFramebufferCreateInfo framebuffer_create_info {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = m_render_pass,
            .attachmentCount = 1u,
            .pAttachments = &image_data.view,
            .width = m_extent.width,
            .height = m_extent.height,
            .layers = 1u
        };

        DEBUG_ASSERT(vkCreateFramebuffer(api.m_instance->get_device(), &framebuffer_create_info, nullptr, &m_framebuffers[i]) == VK_SUCCESS);
    }
}

void UIPass::process(const RenderAPI &api, Handle<CommandList> command_list, UIPassDrawFn draw_fn, u32 swapchain_target_index, Renderer &renderer, World &world) {
    if(draw_fn == nullptr) {
        return;
    }

    const CommandList &cmd_raw = api.m_command_manager->get_command_list_data(command_list);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Draw Commands
    draw_fn(renderer, world);

    ImGui::EndFrame();
    ImGui::Render();

    VkRenderPassBeginInfo render_pass_begin_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_render_pass,
        .framebuffer = m_framebuffers[swapchain_target_index],
        .renderArea {
            .extent = m_extent
        }
    };

    vkCmdBeginRenderPass(cmd_raw.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_raw.command_buffer);
    vkCmdEndRenderPass(cmd_raw.command_buffer);
}
