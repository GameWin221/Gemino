#ifndef GEMINO_IMAGE_MANAGER_HPP
#define GEMINO_IMAGE_MANAGER_HPP

#include <unordered_map>

#include <vk_mem_alloc.h>
#include <common/types.hpp>

struct Image {
    VkImage image{};
    VkImageType image_type{};

    VkImageView view{};
    VkImageViewType view_type{};

    VkFormat format{};
    VkExtent3D extent{};

    VkImageLayout initial_layout{};

    VkImageUsageFlags usage_flags{};
    VkImageAspectFlags aspect_flags{};
    VkImageCreateFlags create_flags{};

    u32 mip_level_count{};
    u32 array_layer_count{};

    VmaAllocation allocation{};
};

struct ImageCreateInfo {
    VkFormat format{};
    VkExtent3D extent{};

    VkImageUsageFlags usage_flags{};
    VkImageAspectFlags aspect_flags{};
    VkImageCreateFlags create_flags{};

    u32 mip_level_count = 1U;
    u32 array_layer_count = 1U;
};

class ImageManager {
public:
    ImageManager(VkDevice device, VmaAllocator allocator);
    ~ImageManager();

    ImageManager& operator=(const ImageManager& other) = delete;
    ImageManager& operator=(ImageManager&& other) noexcept = delete;

    Handle<Image> create_image(const ImageCreateInfo& info);

    void destroy_image(Handle<Image> image_handle);

    const Image& get_image_data(Handle<Image> image_handle) const;

private:
    const VkDevice vk_device;
    const VmaAllocator vk_allocator;

    std::unordered_map<Handle<Image>, Image> image_map{};

    u32 allocated_images_count{};
};


#endif