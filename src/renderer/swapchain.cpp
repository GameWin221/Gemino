#include "swapchain.hpp"
#include <algorithm>
#include <common/debug.hpp>

Swapchain::Swapchain(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface, glm::uvec2 desired_extent, const SwapchainConfig& config)
    : vk_device(device), swapchain_usage(config.usage) {
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

    u32 format_count{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);

    std::vector<VkSurfaceFormatKHR> available_formats(static_cast<usize>(format_count));
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, available_formats.data());

    u32 present_mode_count{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);

    std::vector<VkPresentModeKHR> available_present_modes(static_cast<usize>(present_mode_count));
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, available_present_modes.data());

    swapchain_extent = VkExtent2D { desired_extent.x, desired_extent.y };
    swapchain_extent.width = std::clamp(swapchain_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    swapchain_extent.height = std::clamp(swapchain_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    if(swapchain_extent.width != desired_extent.x || swapchain_extent.height != desired_extent.y)
        DEBUG_WARNING("The desired swapchain extent was invalid and was clamped to: (width: " << swapchain_extent.width <<", height: " << swapchain_extent.height << ")")

    u32 desired_image_count = capabilities.minImageCount + 1U;
    if (capabilities.maxImageCount > 0U && desired_image_count > capabilities.maxImageCount)
        desired_image_count = capabilities.maxImageCount;

    swapchain_format = pick_swapchain_format(available_formats);
    swapchain_present_mode = pick_swapchain_present_mode(available_present_modes, static_cast<bool>(config.v_sync));

    VkSwapchainCreateInfoKHR create_info {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = desired_image_count,
        .imageFormat = swapchain_format.format,
        .imageColorSpace = swapchain_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1U,
        .imageUsage = swapchain_usage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = swapchain_present_mode,
        .clipped = VK_TRUE,
    };

    DEBUG_ASSERT(vkCreateSwapchainKHR(device, &create_info, nullptr, &vk_swapchain) == VK_SUCCESS)

    vkGetSwapchainImagesKHR(device, vk_swapchain, &desired_image_count, nullptr);
    swapchain_images.resize(static_cast<usize>(desired_image_count));
    swapchain_image_views.resize(static_cast<usize>(desired_image_count));
    vkGetSwapchainImagesKHR(device, vk_swapchain, &desired_image_count, swapchain_images.data());

    for(usize i{}; i < swapchain_images.size(); ++i) {
        VkImageViewCreateInfo view_create_info {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain_format.format,
            .subresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1U,
                .layerCount = 1U
            }
        };

        DEBUG_ASSERT(vkCreateImageView(device, &view_create_info, nullptr, &swapchain_image_views[i]) == VK_SUCCESS)
    }
}

Swapchain::~Swapchain() {
    for(const auto& view : swapchain_image_views) {
        vkDestroyImageView(vk_device, view, nullptr);
    }

    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
}

VkSurfaceFormatKHR Swapchain::pick_swapchain_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    for (const auto& format : available_formats)
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;

    DEBUG_WARNING("This physical device does not support the required surface format VK_FORMAT_B8G8R8A8_SRGB with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR color space.")
    DEBUG_WARNING("Falling back to the first available format: (format: " << available_formats[0].format << ", colorSpace: " << available_formats[0].colorSpace << ")")

    return available_formats[0];
}

VkPresentModeKHR Swapchain::pick_swapchain_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes, bool v_sync) {
    bool supports_mailbox = std::find(available_present_modes.begin(), available_present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != available_present_modes.end();
    bool supports_immediate = std::find(available_present_modes.begin(), available_present_modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != available_present_modes.end();

    if (v_sync) {
        return VK_PRESENT_MODE_FIFO_KHR;
    } else {
        if (supports_mailbox) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        } else if(supports_immediate) {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        } else {
            DEBUG_WARNING("This physical device does not support non-vsync present modes!")
            DEBUG_WARNING("Falling back to VK_PRESENT_MODE_FIFO_KHR")
            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }
}
