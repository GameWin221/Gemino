#ifndef GEMINO_INSTANCE_HPP
#define GEMINO_INSTANCE_HPP

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <common/types.hpp>
#include <common/debug.hpp>
#include <optional>

#if DEBUG_MODE
#define ENABLE_VALIDATION_DEFINE
static constexpr bool ENABLE_VALIDATION = true;
#else
static constexpr bool ENABLE_VALIDATION = false;
#endif

#define DESIRED_VK_API_VERSION VK_API_VERSION_1_2

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
    explicit Instance(Proxy window_handle);
    ~Instance();

    Instance& operator=(const Instance& other) = delete;
    Instance& operator=(Instance&& other) noexcept = delete;

    VkFormatProperties get_format_properties(VkFormat format) const;
    VkPhysicalDeviceProperties get_physical_device_properties() const;

    VkDevice get_device() const { return vk_device; }
    VkSurfaceKHR get_surface() const { return vk_surface; }
    VkPhysicalDevice get_physical_device() const { return vk_physical_device; }

    VkQueue get_graphics_queue() const { return vk_queue_graphics; }
    VkQueue get_transfer_queue() const { return vk_queue_transfer; }
    VkQueue get_compute_queue() const { return vk_queue_compute; }

    const QueueFamilyIndices& get_queue_family_indices() const { return vk_queue_indices; }

    VmaAllocator get_allocator() const { return vk_allocator; }
private:
    VkInstance vk_instance{};
    VkDevice vk_device{};

    VmaAllocator vk_allocator{};

    VkPhysicalDevice vk_physical_device{};
    VkSurfaceKHR vk_surface{};

    VkQueue vk_queue_graphics{};
    VkQueue vk_queue_transfer{};
    VkQueue vk_queue_compute{};

    QueueFamilyIndices vk_queue_indices{};

    VkDebugUtilsMessengerEXT vk_debug_messenger{};

private:
    void create_instance();
    void create_debug_messenger();
    void create_window_surface(Proxy window_handle);
    void create_physical_device();
    void create_logical_device();
    void create_allocator();

    bool validation_layers_supported();

    u32 rate_device(VkPhysicalDevice device);

    bool check_is_device_suitable(VkPhysicalDevice device);
    bool check_device_extension_support(VkPhysicalDevice device);
    bool check_device_feature_support(VkPhysicalDevice device);
    bool check_device_swapchain_support(VkPhysicalDevice device);

    QueueFamilyIndices get_device_queue_family_indices(VkPhysicalDevice device);
};

#endif