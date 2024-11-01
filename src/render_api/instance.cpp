#include "instance.hpp"
#include <vector>
#include <set>
#include <array>
#include <cstring>
#include <algorithm>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

static constexpr std::array REQUESTED_VALIDATION_LAYER_NAMES {
    "VK_LAYER_KHRONOS_validation"
};

static constexpr std::array REQUESTED_DEVICE_EXTENSION_NAMES {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static constexpr VkPhysicalDeviceVulkan12Features REQUESTED_DEVICE_FEATURES_VK_1_2 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .drawIndirectCount = true,
    .storageBuffer8BitAccess = true,
    .uniformAndStorageBuffer8BitAccess = true,
    .descriptorIndexing = true,
    .shaderSampledImageArrayNonUniformIndexing = true,
    .descriptorBindingUniformBufferUpdateAfterBind = true,
    .descriptorBindingSampledImageUpdateAfterBind = true,
    .descriptorBindingStorageImageUpdateAfterBind = true,
    .descriptorBindingStorageBufferUpdateAfterBind = true,
    //.descriptorBindingUniformTexelBufferUpdateAfterBind = true,
    //.descriptorBindingStorageTexelBufferUpdateAfterBind = true,
    .descriptorBindingUpdateUnusedWhilePending = true,
    .descriptorBindingPartiallyBound = true,
    //.descriptorBindingVariableDescriptorCount = true,
    .runtimeDescriptorArray = true,
    .scalarBlockLayout = true,
};

static constexpr VkPhysicalDeviceVulkan11Features REQUESTED_DEVICE_FEATURES_VK_1_1 {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .pNext = (void*)&REQUESTED_DEVICE_FEATURES_VK_1_2,
    .storageBuffer16BitAccess = true,
    .uniformAndStorageBuffer16BitAccess = true,
    .shaderDrawParameters = true
};

static constexpr VkPhysicalDeviceFeatures REQUESTED_DEVICE_FEATURES_VK_1_0 {
    .multiDrawIndirect = true,
    .samplerAnisotropy = true
};

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data, void* p_user_data) {
    switch (message_severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            DEBUG_LOG("[Diagnostic]: " << p_callback_data->pMessage)
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            DEBUG_LOG("[Info]: " << p_callback_data->pMessage)
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            DEBUG_WARNING("[Warning]: " << p_callback_data->pMessage)
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            DEBUG_ERROR("[Error]: " << p_callback_data->pMessage)
            break;
        default:
            break;
    }

    return VK_FALSE;
}

static constexpr VkDebugUtilsMessengerCreateInfoEXT DEBUG_MESSENGER_CREATE_INFO {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = vk_debug_callback,
};

Instance::Instance(Proxy window_handle) {
    create_instance();
    create_debug_messenger();
    create_window_surface(window_handle);
    create_physical_device();
    create_logical_device();
    create_allocator();
}
Instance::~Instance() {
    vkDeviceWaitIdle(m_device);

    vmaDestroyAllocator(m_allocator);
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

#ifdef ENABLE_VALIDATION_DEFINE
    auto DestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

    DEBUG_ASSERT(DestroyDebugUtilsMessengerEXT != nullptr)

    DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
#endif

    vkDestroyInstance(m_instance, nullptr);
}

bool Instance::check_is_device_suitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = get_device_queue_family_indices(device);

    if(!check_device_extension_support(device)) {
        return false;
    }
    if(!check_device_feature_support(device)) {
        return false;
    }
    if(!check_device_swapchain_support(device)) {
        return false;
    }
    if(!indices.graphics.has_value()) {
        return false;
    }

    return true;
}
bool Instance::check_device_feature_support(VkPhysicalDevice device) {
    VkPhysicalDeviceVulkan12Features supported_features_vk_1_2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan11Features supported_features_vk_1_1{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    VkPhysicalDeviceFeatures2 supported_features_vk_1_0{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

    supported_features_vk_1_0.pNext = reinterpret_cast<void *>(&supported_features_vk_1_1);
    vkGetPhysicalDeviceFeatures2(device, &supported_features_vk_1_0);

    supported_features_vk_1_0.pNext = reinterpret_cast<void *>(&supported_features_vk_1_2);
    vkGetPhysicalDeviceFeatures2(device, &supported_features_vk_1_0);

    std::vector<std::string> unsupported_features{};

    #define REQUIRE_FEATURE(structure, feature) if(!(structure).feature) unsupported_features.emplace_back(#feature)

    REQUIRE_FEATURE(supported_features_vk_1_0.features, samplerAnisotropy);
    REQUIRE_FEATURE(supported_features_vk_1_0.features, multiDrawIndirect);
    REQUIRE_FEATURE(supported_features_vk_1_1, storageBuffer16BitAccess);
    REQUIRE_FEATURE(supported_features_vk_1_1, uniformAndStorageBuffer16BitAccess);
    REQUIRE_FEATURE(supported_features_vk_1_1, shaderDrawParameters);
    REQUIRE_FEATURE(supported_features_vk_1_2, storageBuffer8BitAccess);
    REQUIRE_FEATURE(supported_features_vk_1_2, uniformAndStorageBuffer8BitAccess);
    REQUIRE_FEATURE(supported_features_vk_1_2, descriptorIndexing);
    REQUIRE_FEATURE(supported_features_vk_1_2, shaderSampledImageArrayNonUniformIndexing);
    REQUIRE_FEATURE(supported_features_vk_1_2, descriptorBindingUniformBufferUpdateAfterBind);
    REQUIRE_FEATURE(supported_features_vk_1_2, descriptorBindingSampledImageUpdateAfterBind);
    REQUIRE_FEATURE(supported_features_vk_1_2, descriptorBindingStorageImageUpdateAfterBind);
    REQUIRE_FEATURE(supported_features_vk_1_2, descriptorBindingStorageBufferUpdateAfterBind);
    REQUIRE_FEATURE(supported_features_vk_1_2, descriptorBindingUpdateUnusedWhilePending);
    REQUIRE_FEATURE(supported_features_vk_1_2, descriptorBindingPartiallyBound);
    REQUIRE_FEATURE(supported_features_vk_1_2, runtimeDescriptorArray);
    REQUIRE_FEATURE(supported_features_vk_1_2, scalarBlockLayout);

    if (!unsupported_features.empty()) {
        DEBUG_WARNING("The physical device doesn't support the following required features: ")
        for (const auto &extension : unsupported_features) {
            DEBUG_WARNING("\t" << extension)
        }

        return false;
    }

    return true;
}
bool Instance::check_device_extension_support(VkPhysicalDevice device) {
    uint32_t extension_count{};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(static_cast<usize>(extension_count));
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(REQUESTED_DEVICE_EXTENSION_NAMES.begin(), REQUESTED_DEVICE_EXTENSION_NAMES.end());

    for (const auto &extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    if (!required_extensions.empty()) {
        DEBUG_WARNING("The physical device doesn't support the following extensions: ")
        for (const auto &extension : required_extensions) {
            DEBUG_WARNING("\t" << extension)
        }

        return false;
    }

    return true;
}
bool Instance::check_device_swapchain_support(VkPhysicalDevice device) {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats{};
    std::vector<VkPresentModeKHR> present_modes{};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &capabilities);

    u32 format_count{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

    if(format_count == 0U) {
        DEBUG_WARNING("There are no supported swapchain formats on this physical device!")
        return false;
    }

    formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, formats.data());

    u32 present_mode_count{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, nullptr);

    if(present_mode_count == 0U) {
        DEBUG_WARNING("There are no supported present modes on this physical device!")
        return false;
    }

    present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &present_mode_count, present_modes.data());

    return true;
}

void Instance::create_instance() {
#ifdef ENABLE_VALIDATION_DEFINE
    if (!validation_layers_supported()) {
        DEBUG_PANIC("Validation layers requested but not supported!")
    }
#endif

    uint32_t glfw_extension_count{};
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> required_extensions(static_cast<usize>(glfw_extension_count));
    std::memcpy(required_extensions.data(), glfw_extensions, sizeof(const char*) * required_extensions.size());

#ifdef __APPLE__
    // According to https://vulkan.lunarg.com/doc/sdk/1.3.216.0/mac/getting_started.html
    // "Beginning with the 1.3.216 Vulkan SDK, the Vulkan Loader is strictly enforcing the new VK_KHR_PORTABILITY_subset"
    required_extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    required_extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

#ifdef ENABLE_VALIDATION_DEFINE
        required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    uint32_t supported_extension_count{};
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, nullptr);

    std::vector<VkExtensionProperties> supported_extensions(static_cast<usize>(supported_extension_count));
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, supported_extensions.data());

    std::set<std::string> required_extensions_set(required_extensions.begin(), required_extensions.end());

    for(const auto &supported_ext : supported_extensions) {
        required_extensions_set.erase(supported_ext.extensionName);
    }

    if(!required_extensions_set.empty()) {
        DEBUG_WARNING("The instance doesn't support the following extensions: ")
        for (const auto& ext : required_extensions_set) {
            DEBUG_WARNING(ext)
        }

        DEBUG_PANIC("Instance requirements were not met.")
    }

    VkApplicationInfo app_info {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Gemino Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Gemino Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = DESIRED_VK_API_VERSION,
    };

    VkInstanceCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<u32>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data(),
    };

#ifdef ENABLE_VALIDATION_DEFINE
    create_info.enabledLayerCount = static_cast<u32>(REQUESTED_VALIDATION_LAYER_NAMES.size());
    create_info.ppEnabledLayerNames = REQUESTED_VALIDATION_LAYER_NAMES.data();
    create_info.pNext = &DEBUG_MESSENGER_CREATE_INFO;
#endif

    DEBUG_ASSERT(vkCreateInstance(&create_info, nullptr, &m_instance) == VK_SUCCESS)
}
void Instance::create_debug_messenger() {
#ifdef ENABLE_VALIDATION_DEFINE
    auto CreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    DEBUG_ASSERT(CreateDebugUtilsMessengerEXT != nullptr)

    DEBUG_ASSERT(CreateDebugUtilsMessengerEXT(m_instance, &DEBUG_MESSENGER_CREATE_INFO, nullptr, &m_debug_messenger) == VK_SUCCESS)
#endif
}
void Instance::create_window_surface(Proxy window_handle) {
    DEBUG_ASSERT(glfwCreateWindowSurface(m_instance, reinterpret_cast<GLFWwindow*>(window_handle), nullptr, &m_surface) == VK_SUCCESS)
}
void Instance::create_physical_device() {
    uint32_t device_count{};
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

    if (device_count == 0U)
        DEBUG_PANIC("Failed to enumerate GPUs with Vulkan support!")

    std::vector<VkPhysicalDevice> all_physical_devices(static_cast<usize>(device_count));
    vkEnumeratePhysicalDevices(m_instance, &device_count, all_physical_devices.data());

    std::vector<PhysicalDeviceRated> suitable_physical_devices{};

    for (const auto &device : all_physical_devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        if (check_is_device_suitable(device)) {
            u32 score = rate_device(device);
            suitable_physical_devices.push_back(PhysicalDeviceRated { device, score });

            DEBUG_LOG(properties.deviceName << " (score: " << score << ") is a suitable physical device.")
        }
        else {
            DEBUG_WARNING(properties.deviceName << " is not a suitable physical device.")
        }
    }

    if (suitable_physical_devices.empty()) {
        DEBUG_PANIC("Failed to find any suitable GPU!")
    }

    // Find the best physical device by highest score
    m_physical_device = std::max_element(
        suitable_physical_devices.begin(),
        suitable_physical_devices.end(),
        [](const auto &a, const auto &b){ return a.score < b.score; }
    )->device;

    m_queue_indices = get_device_queue_family_indices(m_physical_device);

    DEBUG_ASSERT(m_physical_device != nullptr)

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physical_device, &properties);

    bool async_transfer_possible = m_queue_indices.transfer.value() != m_queue_indices.graphics.value() && m_queue_indices.transfer.value() != m_queue_indices.compute.value();
    bool async_compute_possible = m_queue_indices.compute.value() != m_queue_indices.graphics.value() && m_queue_indices.compute.value() != m_queue_indices.transfer.value();

    std::string str(properties.deviceName);
    if (str.find("RTX") != std::string::npos || str.find("GTX") != std::string::npos || str.find("NVIDIA") != std::string::npos || str.find("GeForce") != std::string::npos) {
        m_preferred_warp_size = 32u;
    } else {
        m_preferred_warp_size = 64u;
    }

    DEBUG_LOG("\n" << properties.deviceName << " will be used as the physical device.")
    DEBUG_LOG("Graphics queue index: " << m_queue_indices.graphics.value())
    DEBUG_LOG("Transfer queue index: " << m_queue_indices.transfer.value() << ", async transfer will " << (async_transfer_possible ? "" : "not ") << "be possible.")
    DEBUG_LOG("Compute queue index: " << m_queue_indices.compute.value() << ", async compute will " << (async_compute_possible ? "" : "not ") << "be possible.")
    DEBUG_LOG("Preferred warp size: " << m_preferred_warp_size)
}
void Instance::create_logical_device() {
    std::set<u32> unique_queue_indices {
        m_queue_indices.graphics.value(),
        m_queue_indices.compute.value(),
        m_queue_indices.transfer.value()
    };

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos{};

    f32 queue_priority = 1.0f;
    for (u32 family_idx : unique_queue_indices) {
        queue_create_infos.push_back(VkDeviceQueueCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family_idx,
            .queueCount = 1U,
            .pQueuePriorities = &queue_priority,
        });
    }

    VkDeviceCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &REQUESTED_DEVICE_FEATURES_VK_1_1,
        .queueCreateInfoCount = static_cast<u32>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = static_cast<u32>(REQUESTED_DEVICE_EXTENSION_NAMES.size()),
        .ppEnabledExtensionNames = REQUESTED_DEVICE_EXTENSION_NAMES.data(),
        .pEnabledFeatures = &REQUESTED_DEVICE_FEATURES_VK_1_0,
    };

#ifdef ENABLE_VALIDATION_DEFINE
    create_info.enabledLayerCount = static_cast<u32>(REQUESTED_VALIDATION_LAYER_NAMES.size());
    create_info.ppEnabledLayerNames = REQUESTED_VALIDATION_LAYER_NAMES.data();
#endif

    DEBUG_ASSERT(vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) == VK_SUCCESS)

    vkGetDeviceQueue(m_device, m_queue_indices.graphics.value(), 0U, &m_queue_graphics);
    vkGetDeviceQueue(m_device, m_queue_indices.transfer.value(), 0U, &m_queue_transfer);
    vkGetDeviceQueue(m_device, m_queue_indices.compute.value(), 0U, &m_queue_compute);
}
void Instance::create_allocator() {
    VmaAllocatorCreateInfo create_info {
        .physicalDevice = m_physical_device,
        .device = m_device,
        .instance = m_instance,
        .vulkanApiVersion = DESIRED_VK_API_VERSION
    };

    vmaCreateAllocator(&create_info, &m_allocator);
}

u32 Instance::rate_device(VkPhysicalDevice device) {
    QueueFamilyIndices indices = get_device_queue_family_indices(device);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);

    u32 queue_family_count{};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    u32 score{};

    score += static_cast<u32>(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 800U;
    score += static_cast<u32>(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) * 100U;
    score += static_cast<u32>(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) * 10U;

    score += static_cast<u32>(indices.graphics.value() != indices.transfer.value()) * 600U;
    score += static_cast<u32>(indices.graphics.value() != indices.compute.value()) * 400U;

    return score;
}

QueueFamilyIndices Instance::get_device_queue_family_indices(VkPhysicalDevice device) {
    QueueFamilyIndices indices{};

    u32 queue_family_count{};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(static_cast<usize>(queue_family_count));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    // Find unique graphics and present queue
    for (u32 i{}; i < queue_family_count; ++i) {
        VkBool32 present_support{};
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_support) {
            indices.graphics = i;
            break;
        }
    }

    DEBUG_ASSERT(indices.graphics.has_value())

    // Find unique transfer-only queue
    for (u32 i{}; i < queue_family_count; ++i) {
        if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && i != indices.graphics.value()) {
            indices.transfer = i;
            break;
        }
    }

    // If there was no transfer-only queue found
    if (!indices.transfer.has_value()) {
        for (u32 i{}; i < queue_family_count; ++i) {
            if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && i != indices.graphics.value()) {
                indices.transfer = i;
                break;
            }
        }
    }

    // If there is no unique transfer queue at all
    if (!indices.transfer.has_value()) {
        indices.transfer = indices.graphics.value();
    }

    DEBUG_ASSERT(indices.transfer.has_value())

    // Find unique compute queue
    for (u32 i{}; i < queue_family_count; ++i) {
        if ((queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && i != indices.graphics.value() && i != indices.transfer.value_or(UINT32_MAX)) {
            indices.compute = i;
            break;
        }
    }

    // If there is no unique compute queue
    if(!indices.compute.has_value()) {
        indices.compute = indices.graphics.value();
    }

    DEBUG_ASSERT(indices.compute.has_value())

    return indices;
}

bool Instance::validation_layers_supported() {
    uint32_t layer_count{};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(static_cast<usize>(layer_count));
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    std::set<std::string> requested_validation_layers(REQUESTED_VALIDATION_LAYER_NAMES.begin(), REQUESTED_VALIDATION_LAYER_NAMES.end());

    for(const auto& layer : available_layers) {
        requested_validation_layers.erase(layer.layerName);
    }

    if (!requested_validation_layers.empty()) {
        DEBUG_WARNING("The instance doesn't support the following validation layers: ")
        for (const auto& layer : requested_validation_layers) {
            DEBUG_WARNING(layer)
        }

        return false;
    }

    return true;
}

VkFormatProperties Instance::get_format_properties(VkFormat format) const {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &properties);

    return properties;
}

VkPhysicalDeviceProperties Instance::get_physical_device_properties_vk_1_0() const {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_physical_device, &properties);

    return properties;
}
VkPhysicalDeviceVulkan11Properties Instance::get_physical_device_properties_vk_1_1() const {
    VkPhysicalDeviceVulkan11Properties properties_vk_1_1{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &properties_vk_1_1
    };

    vkGetPhysicalDeviceProperties2(m_physical_device, &properties);

    return properties_vk_1_1;
}
VkPhysicalDeviceVulkan12Properties Instance::get_physical_device_properties_vk_1_2() const {
    VkPhysicalDeviceVulkan12Properties properties_vk_1_2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &properties_vk_1_2
    };

    vkGetPhysicalDeviceProperties2(m_physical_device, &properties);

    return properties_vk_1_2;
}

VkPhysicalDeviceFeatures Instance::get_physical_device_features_vk_1_0() const {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(m_physical_device, &features);

    return features;
}
VkPhysicalDeviceVulkan11Features Instance::get_physical_device_features_vk_1_1() const {
    VkPhysicalDeviceVulkan11Features features_vk_1_1{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    };
    VkPhysicalDeviceFeatures2 features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features_vk_1_1
    };

    vkGetPhysicalDeviceFeatures2(m_physical_device, &features);

    return features_vk_1_1;
}
VkPhysicalDeviceVulkan12Features Instance::get_physical_device_features_vk_1_2() const {
    VkPhysicalDeviceVulkan12Features features_vk_1_2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
    };
    VkPhysicalDeviceFeatures2 features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features_vk_1_2
    };

    vkGetPhysicalDeviceFeatures2(m_physical_device, &features);

    return features_vk_1_2;
}
