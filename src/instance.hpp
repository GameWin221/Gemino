#ifndef GEMINO_INSTANCE_HPP
#define GEMINO_INSTANCE_HPP

#include <optional>
#include <vulkan/vulkan.h>
#include <types.hpp>

#ifdef _DEBUG
#define ENABLE_VALIDATION_DEFINE
static constexpr bool ENABLE_VALIDATION = true;
#else
static constexpr bool ENABLE_VALIDATION = false;
#endif

struct PhysicalDeviceRated {
    VkPhysicalDevice device{};
    u32 score{};
};

struct QueueFamilyIndices {
    std::optional<u32> graphics{};
    std::optional<u32> transfer{};
    std::optional<u32> compute{};
};

class Instance {
public:
    Instance(const Proxy window_handle);

    void destroy();

private:
    VkInstance vk_instance{};
    VkDevice vk_device{};

    VkPhysicalDevice vk_physical_device{};
    QueueFamilyIndices vk_queue_indices{};
    VkSurfaceKHR vk_surface{};

    VkQueue vk_queue_graphics{};
    VkQueue vk_queue_transfer{};
    VkQueue vk_queue_compute{};

    VkDebugUtilsMessengerEXT vk_debug_messenger{};

private:
    void create_instance();
    void create_debug_messenger();
    void create_window_surface(const Proxy window_handle);
    void create_physical_device();
    void create_logical_device();

    bool validation_layers_supported();

    u32 rate_device(VkPhysicalDevice device);

    bool check_is_device_suitable(VkPhysicalDevice device);
    bool check_device_extension_support(VkPhysicalDevice device);
    bool check_device_feature_support(VkPhysicalDevice device);
    bool check_device_swapchain_support(VkPhysicalDevice device);

    QueueFamilyIndices get_device_queue_family_indices(VkPhysicalDevice device);
};

#endif