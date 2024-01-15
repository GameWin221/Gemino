#ifndef GEMINO_RESOURCE_MANAGER_HPP
#define GEMINO_RESOURCE_MANAGER_HPP

#include <vk_mem_alloc.h>
#include <common/types.hpp>
#include <renderer/managers/handle_allocator.hpp>

struct Buffer {
    VkBuffer buffer{};
    VmaAllocation allocation{};

    VkDeviceSize size{};

    VkBufferUsageFlags usage_flags{};
};
struct BufferCreateInfo {
    VkDeviceSize size{};
    VkBufferUsageFlags buffer_usage_flags{};
    VmaMemoryUsage memory_usage_flags{};
};

struct Image {
    VkImage image{};
    VkImageType image_type{};

    VkImageView view{};
    VkImageViewType view_type{};

    VkFormat format{};
    VkExtent3D extent{};

    //VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

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

struct SamplerCreateInfo {
    VkFilter filter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    float mipmap_bias{};
    float anisotropy{};
};
struct Sampler {
    VkSampler sampler{};
};

struct DescriptorBindingUpdateInfo{
    u32 binding_index{};

    struct {
        Handle<Image> image_handle = INVALID_HANDLE;
        Handle<Sampler> image_sampler = INVALID_HANDLE;
    } image_info;

    struct {
        Handle<Buffer> buffer_handle = INVALID_HANDLE;
    } buffer_info;
};
struct DescriptorUpdateInfo{
    std::vector<DescriptorBindingUpdateInfo> bindings{};
};
struct DescriptorBindingCreateInfo {
    VkDescriptorType descriptor_type{};
    u32 count = 1U;
};
struct DescriptorCreateInfo {
    std::vector<DescriptorBindingCreateInfo> bindings{};
};
struct Descriptor {
    VkDescriptorSet set{};
    VkDescriptorSetLayout layout{};

    DescriptorCreateInfo create_info{};
};

class ResourceManager {
public:
    ResourceManager(VkDevice device, VmaAllocator allocator);
    ~ResourceManager();

    ResourceManager& operator=(const ResourceManager& other) = delete;
    ResourceManager& operator=(ResourceManager&& other) noexcept = delete;

    Handle<Image> create_image(const ImageCreateInfo& info);
    Handle<Buffer> create_buffer(const BufferCreateInfo& info);
    Handle<Descriptor> create_descriptor(const DescriptorCreateInfo& info);
    Handle<Sampler> create_sampler(const SamplerCreateInfo& info);

    void* map_buffer(Handle<Buffer> buffer_handle);
    void unmap_buffer(Handle<Buffer> buffer_handle);

    // Only sets the struct value
    //void update_image_layout(Handle<Image> image_handle, VkImageLayout new_layout);
    void update_descriptor(Handle<Descriptor> descriptor_handle, const DescriptorUpdateInfo& info);

    void destroy_image(Handle<Image> image_handle);
    void destroy_buffer(Handle<Buffer> buffer_handle);
    void destroy_descriptor(Handle<Descriptor> descriptor_handle);
    void destroy_sampler(Handle<Sampler> sampler_handle);

    const Image& get_image_data(Handle<Image> image_handle) const;
    const Buffer& get_buffer_data(Handle<Buffer> buffer_handle) const;
    const Descriptor& get_descriptor_data(Handle<Descriptor> descriptor_handle) const;
    const Sampler& get_sampler_data(Handle<Sampler> sampler_handle) const;

    const u32 per_descriptor_pool_size = 32U;
    //const u32 max_descriptor_sets = 256U;

private:
    const VkDevice vk_device;
    const VmaAllocator vk_allocator;

    VkDescriptorPool vk_descriptor_pool{};

    HandleAllocator<Buffer> buffer_allocator{};
    HandleAllocator<Image> image_allocator{};
    HandleAllocator<Descriptor> descriptor_allocator{};
    HandleAllocator<Sampler> sampler_allocator{};
};

#endif
