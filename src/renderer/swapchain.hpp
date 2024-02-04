#ifndef GEMINO_SWAPCHAIN_HPP
#define GEMINO_SWAPCHAIN_HPP

#include <vulkan/vulkan.h>
#include <common/types.hpp>
#include <unordered_set>
#include <vector>
#include <glm/vec2.hpp>

enum struct VSyncMode : u32 {
    Disabled = 0U, // Immediate or Mailbox - Tearing dependent on the GPU - Does not limit fps
    Enabled,       // Fifo                 - No tearing - Limits fps to vsync intervals
    Adaptive,      // Relaxed Fifo         - Tearing dependent on frame time - Limits fps unless the frame time is lower than vsync period
};

struct SwapchainConfig {
    VSyncMode v_sync = VSyncMode::Enabled;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
};

class Swapchain {
public:
    Swapchain(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface, glm::uvec2 desired_extent, const SwapchainConfig& config);
    ~Swapchain();

    Swapchain& operator=(const Swapchain& other) = delete;
    Swapchain& operator=(Swapchain&& other) noexcept = delete;

    VkExtent2D get_extent() const { return swapchain_extent; }
    VkFormat get_format() const { return swapchain_format.format; }
    VkImageUsageFlags get_usage() const { return swapchain_usage; }

    std::unordered_set<VSyncMode> get_available_vsync_modes() const;

    const std::vector<VkImageView>& get_image_views() const { return swapchain_image_views; };
    const std::vector<VkImage>& get_images() const { return swapchain_images; };

    VkSwapchainKHR get_handle() const { return vk_swapchain; }

private:
    const VkDevice vk_device;
    const VkPhysicalDevice vk_physical_device;
    const VkSurfaceKHR vk_surface;

    VkSwapchainKHR vk_swapchain{};

    std::vector<VkImage> swapchain_images{};
    std::vector<VkImageView> swapchain_image_views{};

    VkSurfaceFormatKHR swapchain_format{};
    VkPresentModeKHR swapchain_present_mode{};
    VkImageUsageFlags swapchain_usage{};
    VkExtent2D swapchain_extent{};

    VkSurfaceFormatKHR pick_swapchain_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR pick_swapchain_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes, VSyncMode v_sync);
};

#endif
