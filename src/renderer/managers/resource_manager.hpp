#ifndef GEMINO_RESOURCE_MANAGER_HPP
#define GEMINO_RESOURCE_MANAGER_HPP

#define VMA_VULKAN_VERSION 1002000 // Vulkan 1.2
#include <vk_mem_alloc.h>
#include <common/types.hpp>
#include <common/handle_allocator.hpp>

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

    VkImageUsageFlags usage_flags{};
    VkImageAspectFlags aspect_flags{};
    VkImageCreateFlags create_flags{};

    u32 mip_level_count{};
    u32 array_layer_count{};

    std::vector<VkImageView> per_mip_views{};

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

    bool create_per_mip_views = false;
};

struct SamplerCreateInfo {
    VkFilter filter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSamplerReductionMode reduction_mode = VK_SAMPLER_REDUCTION_MODE_MAX_ENUM;
    VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    float max_mipmap = VK_LOD_CLAMP_NONE;
    float mipmap_bias{};
    float anisotropy{};
};
struct Sampler {
    VkSampler sampler{};
};

struct DescriptorBindingUpdateInfo{
    u32 binding_index{};
    u32 array_index{};

    struct {
        Handle<Image> image_handle = INVALID_HANDLE;
        Handle<Sampler> image_sampler = INVALID_HANDLE;
        u32 image_mip = INVALID_HANDLE;
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
    Handle<Image> create_image_borrowed(VkImage borrowed_image, VkImageView borrowed_view, const ImageCreateInfo& info);
    Handle<Buffer> create_buffer(const BufferCreateInfo& info);
    Handle<Descriptor> create_descriptor(const DescriptorCreateInfo& info);
    Handle<Sampler> create_sampler(const SamplerCreateInfo& info);

    void* map_buffer(Handle<Buffer> buffer_handle);
    void unmap_buffer(Handle<Buffer> buffer_handle);
    void flush_mapped_buffer(Handle<Buffer> buffer_handle, VkDeviceSize size = 0, VkDeviceSize offset = 0);

    void memcpy_to_buffer_once(Handle<Buffer> buffer_handle, const void* src_data, usize size, usize dst_offset = 0, usize src_offset = 0);

    // Requires an already mapped buffer
    void memcpy_to_buffer(void* dst_mapped_buffer, const void* src_data, usize size, usize dst_offset = 0, usize src_offset = 0);

    void update_descriptor(Handle<Descriptor> descriptor_handle, const DescriptorUpdateInfo& info);

    void destroy_image(Handle<Image> image_handle);
    void destroy_buffer(Handle<Buffer> buffer_handle);
    void destroy_descriptor(Handle<Descriptor> descriptor_handle);
    void destroy_sampler(Handle<Sampler> sampler_handle);

    const Image& get_image_data(Handle<Image> image_handle) const;
    const Buffer& get_buffer_data(Handle<Buffer> buffer_handle) const;
    const Descriptor& get_descriptor_data(Handle<Descriptor> descriptor_handle) const;
    const Sampler& get_sampler_data(Handle<Sampler> sampler_handle) const;

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
