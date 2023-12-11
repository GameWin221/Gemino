#include <instance.hpp>
#include <types.hpp>
#include <debug.hpp>
#include <vector>
#include <array>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

constexpr std::array<const char*, 1> REQUESTED_VALIDATION_LAYER_NAMES {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef _DEBUG
static constexpr bool ENABLE_VALIDATION = true;
#else
static constexpr bool ENABLE_VALIDATION = false;
#endif

static VkInstance vk_instance = nullptr;
static VkDebugUtilsMessengerEXT vk_debug_messenger = nullptr;

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            DEBUG_LOG("[Diagnostic]: " << pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            DEBUG_LOG("[Info]: " << pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            DEBUG_WARNING("[Warning]: " << pCallbackData->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            DEBUG_ERROR("[Error]: " << pCallbackData->pMessage);
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

static bool validation_layers_supported() {
    uint32_t layer_count{};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : REQUESTED_VALIDATION_LAYER_NAMES) {
        if(std::find_if(available_layers.begin(), available_layers.end(), [layer_name](const auto& props){
            return strcmp(props.layerName, layer_name) == 0;
        }) == available_layers.end()) {
            return false;
        }
    }

    return true;
}

static VkInstance create_instance() {
    if (ENABLE_VALIDATION && !validation_layers_supported())
        DEBUG_PANIC("Validation layers requested but not supported!")

    VkApplicationInfo app_info {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Gemino Engine",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "Gemino Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_2,
    };

    uint32_t glfw_extension_count = 0U;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> required_extensions(static_cast<usize>(glfw_extension_count));
    std::memcpy(required_extensions.data(), glfw_extensions, sizeof(const char*) * required_extensions.size());

#ifdef __APPLE__
    // According to https://vulkan.lunarg.com/doc/sdk/1.3.216.0/mac/getting_started.html
    // "Beginning with the 1.3.216 Vulkan SDK, the Vulkan Loader is strictly enforcing the new VK_KHR_PORTABILITY_subset"
    required_extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    required_extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if constexpr (ENABLE_VALIDATION)
        required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    uint32_t supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, nullptr);

    std::vector<VkExtensionProperties> supported_extensions(static_cast<usize>(supported_extension_count));
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, supported_extensions.data());

    DEBUG_LOG("Supported instance extensions:")
    for(const auto& ext : supported_extensions)
        DEBUG_LOG('\t' << ext.extensionName)

    for(const char* ext : required_extensions) {
        if(std::find_if(supported_extensions.begin(), supported_extensions.end(), [ext](const auto& props){
            return strcmp(props.extensionName, ext) == 0;
        }) == supported_extensions.end()) {
            DEBUG_PANIC("Instance extension: " << ext << " is not supported!");
        }
    }

    VkInstanceCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<u32>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()
    };

    if constexpr (ENABLE_VALIDATION) {
        create_info.enabledLayerCount = static_cast<u32>(REQUESTED_VALIDATION_LAYER_NAMES.size());
        create_info.ppEnabledLayerNames = REQUESTED_VALIDATION_LAYER_NAMES.data();
        create_info.pNext = &DEBUG_MESSENGER_CREATE_INFO;
    }

    DEBUG_ASSERT(vkCreateInstance(&create_info, nullptr, &vk_instance) == VK_SUCCESS)
}

static void create_debug_messenger() {
    if constexpr (!ENABLE_VALIDATION) return;

    auto CreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(vk_instance, "vkCreateDebugUtilsMessengerEXT"));

    DEBUG_ASSERT(CreateDebugUtilsMessengerEXT != nullptr)

    DEBUG_ASSERT(CreateDebugUtilsMessengerEXT(vk_instance, &DEBUG_MESSENGER_CREATE_INFO, nullptr, &vk_debug_messenger) == VK_SUCCESS)
}

void Instance::init() {
    create_instance();
    create_debug_messenger();
}
void Instance::deinit() {
    if constexpr (ENABLE_VALIDATION) {
        auto DestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(vk_instance, "vkDestroyDebugUtilsMessengerEXT"));

        DEBUG_ASSERT(DestroyDebugUtilsMessengerEXT != nullptr)

        DestroyDebugUtilsMessengerEXT(vk_instance, vk_debug_messenger, nullptr);
    }

    vkDestroyInstance(vk_instance, nullptr);
}
