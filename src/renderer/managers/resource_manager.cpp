#include "resource_manager.hpp"
#include <cstring>

ResourceManager::ResourceManager(VkDevice device, VmaAllocator allocator) : vk_device(device), vk_allocator(allocator) {
    std::vector<VkDescriptorPoolSize> pool_sizes {
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER, 128U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4096U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 0U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256U },
        VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 256U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0U },
        //VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0U }
    };

    u32 max_sets{};
    for(const auto& size : pool_sizes) {
        max_sets += size.descriptorCount;
    }

    VkDescriptorPoolCreateInfo descriptor_pool_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data()
    };

    DEBUG_ASSERT(vkCreateDescriptorPool(vk_device, &descriptor_pool_create_info, nullptr, &vk_descriptor_pool) == VK_SUCCESS)
}
ResourceManager::~ResourceManager() {
    for(const auto& handle : image_allocator.get_valid_handles_copy()) {
        destroy_image(handle);
    }
    for(const auto& handle : buffer_allocator.get_valid_handles_copy()) {
        destroy_buffer(handle);
    }
    for(const auto& handle : descriptor_allocator.get_valid_handles_copy()) {
        destroy_descriptor(handle);
    }
    for(const auto& handle : sampler_allocator.get_valid_handles_copy()) {
        destroy_sampler(handle);
    }

    vkDestroyDescriptorPool(vk_device, vk_descriptor_pool, nullptr);
}

Handle<Image> ResourceManager::create_image(const ImageCreateInfo& info) {
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
        .extent = image_extent,
        .usage_flags = info.usage_flags,
        .aspect_flags = info.aspect_flags,
        .create_flags = info.create_flags,
        .mip_level_count = info.mip_level_count,
        .array_layer_count = info.array_layer_count,
    };

    if(info.create_flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT) {
        image.image_type = VK_IMAGE_TYPE_3D;
    }

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

    return image_allocator.alloc(image);
}
Handle<Buffer> ResourceManager::create_buffer(const BufferCreateInfo &info) {
    Buffer buffer{
        .size = info.size,
        .usage_flags = info.buffer_usage_flags,
    };

    VkBufferCreateInfo buffer_create_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = buffer.size,
        .usage = buffer.usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocation_create_info{
        .usage = info.memory_usage_flags
    };

    DEBUG_ASSERT(vmaCreateBuffer(vk_allocator, &buffer_create_info, &allocation_create_info, &buffer.buffer, &buffer.allocation, nullptr) == VK_SUCCESS)

    return buffer_allocator.alloc(buffer);
}
Handle<Descriptor> ResourceManager::create_descriptor(const DescriptorCreateInfo &info) {
    Descriptor descriptor{
        .create_info = info
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings{};
    for(const auto& binding : info.bindings) {
        bindings.push_back(VkDescriptorSetLayoutBinding{
            .binding = static_cast<u32>(bindings.size()),
            .descriptorType = binding.descriptor_type,
            .descriptorCount = binding.count,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT
        });
    }

    std::vector<VkDescriptorBindingFlags> binding_flags(bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
    // If needed for variable descriptor count
    //for (const auto& binding : bindings) {
    //    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
    //        binding_flags[binding.binding] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
    //    }
    //}

    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<u32>(binding_flags.size()),
        .pBindingFlags = binding_flags.data()
    };

    VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &extended_create_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    };

    DEBUG_ASSERT(vkCreateDescriptorSetLayout(vk_device, &descriptor_layout_create_info, nullptr, &descriptor.layout) == VK_SUCCESS)

    VkDescriptorSetAllocateInfo allocate_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk_descriptor_pool,
        .descriptorSetCount = 1U,
        .pSetLayouts = &descriptor.layout
    };

    DEBUG_ASSERT(vkAllocateDescriptorSets(vk_device, &allocate_info, &descriptor.set) == VK_SUCCESS)

    return descriptor_allocator.alloc(descriptor);
}
Handle<Sampler> ResourceManager::create_sampler(const SamplerCreateInfo &info) {
    Sampler sampler{};

    VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = info.filter,
        .minFilter = info.filter,
        .mipmapMode = info.mipmap_mode,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = info.mipmap_bias,
        .anisotropyEnable = (info.anisotropy > 0.0f),
        .maxAnisotropy = std::min(info.anisotropy, 16.0f),
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = info.max_mipmap,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    DEBUG_ASSERT(vkCreateSampler(vk_device, &create_info, nullptr, &sampler.sampler) == VK_SUCCESS)

    return sampler_allocator.alloc(sampler);
}

void* ResourceManager::map_buffer(Handle<Buffer> buffer_handle) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot map buffer - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    void* data;
    DEBUG_ASSERT(vmaMapMemory(vk_allocator, buffer_allocator.get_element(buffer_handle).allocation, &data) == VK_SUCCESS)

    return data;
}
void ResourceManager::unmap_buffer(Handle<Buffer> buffer_handle) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot unmap buffer - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    vmaUnmapMemory(vk_allocator, buffer_allocator.get_element(buffer_handle).allocation);
}
void ResourceManager::memcpy_to_buffer_once(Handle<Buffer> buffer_handle, const void* src_data, usize size, usize dst_offset, usize src_offset) {
    void* mapped = reinterpret_cast<void*>(reinterpret_cast<usize>(map_buffer(buffer_handle)) + dst_offset);
    void* src = reinterpret_cast<void*>(reinterpret_cast<usize>(src_data) + src_offset);

    std::memcpy(mapped, src, size);

    unmap_buffer(buffer_handle);
}
void ResourceManager::memcpy_to_buffer(void *dst_mapped_buffer, const void *src_data, usize size, usize dst_offset, usize src_offset) {
    void* mapped = reinterpret_cast<void*>(reinterpret_cast<usize>(dst_mapped_buffer) + dst_offset);
    void* src = reinterpret_cast<void*>(reinterpret_cast<usize>(src_data) + src_offset);
    std::memcpy(mapped, src, size);
}


/*
void ResourceManager::update_image_layout(Handle<Image> image_handle, VkImageLayout new_layout) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!image_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot update image layout - Image with a handle id: = " << image_handle << ", does not exist!")
    }
#endif

    image_allocator.get_element_mutable(image_handle).current_layout = new_layout;
}
*/
void ResourceManager::update_descriptor(Handle<Descriptor> descriptor_handle, const DescriptorUpdateInfo &info) {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!descriptor_allocator.is_handle_valid(descriptor_handle)) {
        DEBUG_PANIC("Cannot update descriptor - Descriptor with a handle id: = " << descriptor_handle << ", does not exist!")
    }
#endif

    const Descriptor& descriptor = descriptor_allocator.get_element(descriptor_handle);

    std::vector<VkWriteDescriptorSet> descriptor_writes{};
    std::vector<u32> descriptor_info_indices{};

    std::vector<VkDescriptorImageInfo> descriptor_images{};
    std::vector<VkDescriptorBufferInfo> descriptor_buffers{};

    for(const auto& binding : info.bindings) {
#if DEBUG_MODE
        if(binding.binding_index >= static_cast<u32>(descriptor.create_info.bindings.size())) {
            DEBUG_PANIC("Binding write failed! - Binding index out of range, binding_index = " << binding.binding_index << ", bindings.size() = " << static_cast<u32>(descriptor.create_info.bindings.size()))
        }
        if(binding.array_index >= descriptor.create_info.bindings[binding.binding_index].count) {
            DEBUG_PANIC("Binding write failed! - Binding array index out of range, array_index = " << binding.array_index << ", descriptor.create_info.bindings[binding.binding_index].count = " << descriptor.create_info.bindings[binding.binding_index].count)
        }
#endif

        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor.set,
            .dstBinding = binding.binding_index,
            .dstArrayElement = binding.array_index,
            .descriptorCount = 1U,
            .descriptorType = descriptor.create_info.bindings[binding.binding_index].descriptor_type
        };

        if(binding.buffer_info.buffer_handle != INVALID_HANDLE && binding.image_info.image_handle == INVALID_HANDLE) {
            descriptor_info_indices.push_back(static_cast<u32>(descriptor_buffers.size()));
            descriptor_buffers.push_back(VkDescriptorBufferInfo{
                .buffer = get_buffer_data(binding.buffer_info.buffer_handle).buffer,
                .range = get_buffer_data(binding.buffer_info.buffer_handle).size
            });
        } else if(binding.buffer_info.buffer_handle == INVALID_HANDLE && binding.image_info.image_handle != INVALID_HANDLE) {
            descriptor_info_indices.push_back(static_cast<u32>(descriptor_images.size()));
            descriptor_images.push_back(VkDescriptorImageInfo {
                .sampler = get_sampler_data(binding.image_info.image_sampler).sampler,
                .imageView = get_image_data(binding.image_info.image_handle).view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });
        } else {
            DEBUG_PANIC("Binding write failed! - Both or none binding handles were valid, binding_index = " << binding.binding_index)
        }

        descriptor_writes.push_back(write);
    }

    for(usize i{}; i < descriptor_writes.size(); ++i) {
        auto& write = descriptor_writes[i];

        if(write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || write.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            write.pImageInfo = &descriptor_images[descriptor_info_indices[i]];
        } else if(write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            write.pBufferInfo = &descriptor_buffers[descriptor_info_indices[i]];
        } else {
            DEBUG_PANIC("Binding write failed! - Invalid descriptor type, dstBinding = " << write.dstBinding << ", dstArrayElement = "<< write.dstArrayElement <<", descriptorType = " << write.descriptorType)
        }
    }

    vkUpdateDescriptorSets(vk_device, static_cast<u32>(descriptor_writes.size()), descriptor_writes.data(), 0U, nullptr);
}

void ResourceManager::destroy_image(Handle<Image> image_handle) {
    if (!image_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot delete image - Image with a handle id: = " << image_handle << ", does not exist!")
    }

    const Image& image = image_allocator.get_element(image_handle);

    vkDestroyImageView(vk_device, image.view, nullptr);
    vmaDestroyImage(vk_allocator, image.image, image.allocation);

    image_allocator.free(image_handle);
}
void ResourceManager::destroy_buffer(Handle<Buffer> buffer_handle) {
    if (!buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot delete buffer - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }

    const Buffer& buffer = buffer_allocator.get_element(buffer_handle);

    vmaDestroyBuffer(vk_allocator, buffer.buffer, buffer.allocation);

    buffer_allocator.free(buffer_handle);
}
void ResourceManager::destroy_descriptor(Handle<Descriptor> descriptor_handle) {
    if (!descriptor_allocator.is_handle_valid(descriptor_handle)) {
        DEBUG_PANIC("Cannot delete descriptor - Descriptor with a handle id: = " << descriptor_handle << ", does not exist!")
    }

    const Descriptor& descriptor = descriptor_allocator.get_element(descriptor_handle);

    vkFreeDescriptorSets(vk_device, vk_descriptor_pool, 1U, &descriptor.set);
    vkDestroyDescriptorSetLayout(vk_device, descriptor.layout, nullptr);

    buffer_allocator.free(descriptor_handle);
}
void ResourceManager::destroy_sampler(Handle<Sampler> sampler_handle) {
    if (!sampler_allocator.is_handle_valid(sampler_handle)) {
        DEBUG_PANIC("Cannot delete sampler - Sampler with a handle id: = " << sampler_handle << ", does not exist!")
    }

    const Sampler& sampler = sampler_allocator.get_element(sampler_handle);

    vkDestroySampler(vk_device, sampler.sampler, nullptr);

    sampler_allocator.free(sampler_handle);
}

const Image& ResourceManager::get_image_data(Handle<Image> image_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!image_allocator.is_handle_valid(image_handle)) {
        DEBUG_PANIC("Cannot get image data - Image with a handle id: = " << image_handle << ", does not exist!")
    }
#endif

    return image_allocator.get_element(image_handle);
}
const Buffer& ResourceManager::get_buffer_data(Handle<Buffer> buffer_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!buffer_allocator.is_handle_valid(buffer_handle)) {
        DEBUG_PANIC("Cannot get buffer data - Buffer with a handle id: = " << buffer_handle << ", does not exist!")
    }
#endif

    return buffer_allocator.get_element(buffer_handle);
}
const Descriptor& ResourceManager::get_descriptor_data(Handle<Descriptor> descriptor_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!descriptor_allocator.is_handle_valid(descriptor_handle)) {
        DEBUG_PANIC("Cannot get descriptor data - Descriptor with a handle id: = " << descriptor_handle << ", does not exist!")
    }
#endif

    return descriptor_allocator.get_element(descriptor_handle);
}
const Sampler& ResourceManager::get_sampler_data(Handle<Sampler> sampler_handle) const {
#if DEBUG_MODE // Remove hot-path checks in release mode
    if (!sampler_allocator.is_handle_valid(sampler_handle)) {
        DEBUG_PANIC("Cannot get sampler data - Sampler with a handle id: = " << sampler_handle << ", does not exist!")
    }
#endif

    return sampler_allocator.get_element(sampler_handle);
}