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

    Instance &operator=(const Instance &other) = delete;
    Instance &operator=(Instance &&other) noexcept = delete;

    VkFormatProperties get_format_properties(VkFormat format) const;

    VkPhysicalDeviceProperties get_physical_device_properties_vk_1_0() const;
    VkPhysicalDeviceVulkan11Properties get_physical_device_properties_vk_1_1() const;
    VkPhysicalDeviceVulkan12Properties get_physical_device_properties_vk_1_2() const;

    VkPhysicalDeviceFeatures get_physical_device_features_vk_1_0() const;
    VkPhysicalDeviceVulkan11Features get_physical_device_features_vk_1_1() const;
    VkPhysicalDeviceVulkan12Features get_physical_device_features_vk_1_2() const;

    u32 get_physical_device_preferred_warp_size() const { return m_preferred_warp_size; };

    VkInstance get_instance() const { return m_instance; }
    VkDevice get_device() const { return m_device; }
    VkSurfaceKHR get_surface() const { return m_surface; }
    VkPhysicalDevice get_physical_device() const { return m_physical_device; }

    VkQueue get_graphics_queue() const { return m_queue_graphics; }
    VkQueue get_transfer_queue() const { return m_queue_transfer; }
    VkQueue get_compute_queue() const { return m_queue_compute; }

    const QueueFamilyIndices &get_queue_family_indices() const { return m_queue_indices; }

    VmaAllocator get_allocator() const { return m_allocator; }

private:
    VkInstance m_instance{};
    VkDevice m_device{};

    VmaAllocator m_allocator{};

    VkPhysicalDevice m_physical_device{};
    VkSurfaceKHR m_surface{};

    VkQueue m_queue_graphics{};
    VkQueue m_queue_transfer{};
    VkQueue m_queue_compute{};

    QueueFamilyIndices m_queue_indices{};

    VkDebugUtilsMessengerEXT m_debug_messenger{};

    u32 m_preferred_warp_size{};

    void create_instance();
    void create_debug_messenger();
    void create_window_surface(Proxy window_handle);
    void create_physical_device();
    void create_logical_device();
    void create_allocator();

    bool validation_layers_supported();

    u32 rate_device(VkPhysicalDevice device);

    bool check_is_device_suitable(VkPhysicalDevice device);
    bool check_device_feature_support(VkPhysicalDevice device);
    bool check_device_extension_support(VkPhysicalDevice device);
    bool check_device_swapchain_support(VkPhysicalDevice device);

    QueueFamilyIndices get_device_queue_family_indices(VkPhysicalDevice device);
};

#endif