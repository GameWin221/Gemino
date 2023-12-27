#ifndef GEMINO_SWAPCHAIN_HPP
#define GEMINO_SWAPCHAIN_HPP

#include <vulkan/vulkan.h>
#include <common/types.hpp>
#include <vector>
#include <glm/vec2.hpp>

enum struct VSyncMode : bool {
    Disabled = false,
    Enabled = true
};

class Swapchain {
public:
    Swapchain(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface, glm::uvec2 desired_extent, VSyncMode v_sync);
    ~Swapchain();

    Swapchain& operator=(const Swapchain& other) = delete;
    Swapchain& operator=(Swapchain&& other) noexcept = delete;

    VkExtent2D get_extent() const { return swapchain_extent; }
    VkFormat get_format() const { return swapchain_format.format; }

    const std::vector<VkImageView>& get_image_views() const { return swapchain_image_views; };

    VkSwapchainKHR get_handle() const { return vk_swapchain; }

private:
    const VkDevice vk_device;

    VkSwapchainKHR vk_swapchain{};

    std::vector<VkImage> swapchain_images{};
    std::vector<VkImageView> swapchain_image_views{};

    VkSurfaceFormatKHR swapchain_format{};
    VkExtent2D swapchain_extent{};
    VkPresentModeKHR swapchain_present_mode{};

    VkSurfaceFormatKHR pick_swapchain_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR pick_swapchain_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes, bool v_sync);
};

#endif
