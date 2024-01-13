#include "image_manager.hpp"
#include <algorithm>
#include <common/debug.hpp>

ImageManager::ImageManager(VkDevice device, VmaAllocator allocator)
    : vk_allocator(allocator), vk_device(device) {

}

ImageManager::~ImageManager() {
    for(const auto& handle : handle_allocator.get_valid_handles()) {
        destroy_image(handle);
    }
}

Handle<Image> ImageManager::create_image(const ImageCreateInfo& info) {
    VkExtent3D image_extent{
        .width = std::max(1U, info.extent.width),
        .height = std::max(1U, info.extent.height),
        .depth = std::max(1U, info.extent.depth)
    };

    u32 dimension_count =
        static_cast<u32>((image_extent.width > 1U)) +
        static_cast<u32>((image_extent.height > 1U)) +
        static_cast<u32>((image_extent.depth > 1U));

    DEBUG_ASSERT(dimension_count > 0U)

    Image image{
        .image_type = static_cast<VkImageType>(dimension_count - 1U),
        .format = info.format,
        .initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage_flags = info.usage_flags,
        .aspect_flags = info.aspect_flags,
        .create_flags = info.create_flags,
        .mip_level_count = info.mip_level_count,
        .array_layer_count = info.array_layer_count,
    };

    if (info.create_flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
        if (info.array_layer_count % 6 != 0) {
            DEBUG_PANIC("info.array_layer_count for cube textures must be a multiple of 6! Meanwhile info.array_layer_count = " << info.array_layer_count)
        }

        if (info.array_layer_count > 6U) {
            image.view_type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        } else {
            image.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
        }
    } else {
        if (info.array_layer_count > 1U) {
            image.view_type = static_cast<VkImageViewType>(dimension_count - 1U + 4U); // Array types values are always greater by 4 (except for VK_IMAGE_VIEW_TYPE_CUBE)
        } else {
            image.view_type = static_cast<VkImageViewType>(dimension_count - 1U);
        }
    }

    VkImageCreateInfo image_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = image.create_flags,
        .imageType = image.image_type,
        .format = image.format,
        .extent = image_extent,
        .mipLevels = image.mip_level_count,
        .arrayLayers = image.array_layer_count,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = image.usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocation_create_info {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    DEBUG_ASSERT(vmaCreateImage(vk_allocator, &image_create_info, &allocation_create_info, &image.image, &image.allocation, nullptr) == VK_SUCCESS)

    VkImageViewCreateInfo view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.image,
        .viewType = image.view_type,
        .format = image.format,

        .subresourceRange {
            .aspectMask = image.aspect_flags,
            .levelCount = image.mip_level_count,
            .layerCount = image.array_layer_count
        }
    };

    DEBUG_ASSERT(vkCreateImageView(vk_device, &view_create_info, nullptr, &image.view) == VK_SUCCESS)

    return handle_allocator.alloc(image);
}

void ImageManager::destroy_image(Handle<Image> image_handle) {
    if (!handle_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot delete image - Image with a handle id: = " << image_handle << ", does not exist!")
    }

    const Image& image = handle_allocator.get_element(image_handle);

    vkDestroyImageView(vk_device, image.view, nullptr);
    vmaDestroyImage(vk_allocator, image.image, image.allocation);

    handle_allocator.free(image_handle);
}

const Image &ImageManager::get_image_data(Handle<Image> image_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!handle_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot get image data - Image with a handle id: = " << image_handle << ", does not exist!")
    }
#endif

    return handle_allocator.get_element(image_handle);
}
