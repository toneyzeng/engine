// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

_Pragma("GCC diagnostic push");
_Pragma("GCC diagnostic ignored \"-Wnullability-completeness\"");
_Pragma("GCC diagnostic ignored \"-Wunused-variable\"");
_Pragma("GCC diagnostic ignored \"-Wthread-safety-analysis\"");

#define VMA_IMPLEMENTATION
#include "impeller/renderer/backend/vulkan/allocator_vk.h"
#include "impeller/renderer/backend/vulkan/device_buffer_vk.h"
#include "impeller/renderer/backend/vulkan/formats_vk.h"
#include "impeller/renderer/backend/vulkan/texture_vk.h"

#include <memory>

_Pragma("GCC diagnostic pop");

namespace impeller {

AllocatorVK::AllocatorVK(ContextVK& context,
                         uint32_t vulkan_api_version,
                         const vk::PhysicalDevice& physical_device,
                         const vk::Device& logical_device,
                         const vk::Instance& instance,
                         PFN_vkGetInstanceProcAddr get_instance_proc_address,
                         PFN_vkGetDeviceProcAddr get_device_proc_address)
    : context_(context), device_(logical_device) {
  VmaVulkanFunctions proc_table = {};
  proc_table.vkGetInstanceProcAddr = get_instance_proc_address;
  proc_table.vkGetDeviceProcAddr = get_device_proc_address;

  VmaAllocatorCreateInfo allocator_info = {};
  allocator_info.vulkanApiVersion = vulkan_api_version;
  allocator_info.physicalDevice = physical_device;
  allocator_info.device = logical_device;
  allocator_info.instance = instance;
  allocator_info.pVulkanFunctions = &proc_table;

  VmaAllocator allocator = {};
  auto result = vk::Result{::vmaCreateAllocator(&allocator_info, &allocator)};
  if (result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Could not create memory allocator";
    return;
  }
  allocator_ = allocator;
  is_valid_ = true;
}

AllocatorVK::~AllocatorVK() {
  if (allocator_) {
    ::vmaDestroyAllocator(allocator_);
  }
}

// |Allocator|
bool AllocatorVK::IsValid() const {
  return is_valid_;
}

// |Allocator|
std::shared_ptr<Texture> AllocatorVK::OnCreateTexture(
    const TextureDescriptor& desc) {
  auto image_create_info = vk::ImageCreateInfo{};
  image_create_info.imageType = vk::ImageType::e2D;
  image_create_info.format = ToVKImageFormat(desc.format);
  image_create_info.extent.width = desc.size.width;
  image_create_info.extent.height = desc.size.height;
  image_create_info.samples = ToVKSampleCount(desc.sample_count);
  image_create_info.mipLevels = desc.mip_count;

  // TODO (kaushikiska): should we read these from desc?
  image_create_info.extent.depth = 1;
  image_create_info.arrayLayers = 1;

  image_create_info.tiling = vk::ImageTiling::eOptimal;
  image_create_info.initialLayout = vk::ImageLayout::eUndefined;
  image_create_info.usage = vk::ImageUsageFlagBits::eSampled |
                            vk::ImageUsageFlagBits::eColorAttachment;

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
  // docs recommend using `VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT` for image
  // allocations, but setting them to be host visible for now.
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;

  auto create_info_native =
      static_cast<vk::ImageCreateInfo::NativeType>(image_create_info);

  VkImage img;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
  auto result = vk::Result{vmaCreateImage(allocator_, &create_info_native,
                                          &alloc_create_info, &img, &allocation,
                                          &allocation_info)};
  if (result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Unable to allocate an image";
    return nullptr;
  }

  vk::ImageViewCreateInfo view_create_info = {};
  view_create_info.image = img;
  view_create_info.viewType = vk::ImageViewType::e2D;
  view_create_info.format = image_create_info.format;
  view_create_info.subresourceRange.aspectMask =
      vk::ImageAspectFlagBits::eColor;
  view_create_info.subresourceRange.levelCount = image_create_info.mipLevels;
  view_create_info.subresourceRange.layerCount = image_create_info.arrayLayers;

  auto img_view_res = device_.createImageView(view_create_info);
  if (img_view_res.result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Unable to create an image view: "
                   << vk::to_string(img_view_res.result);
    return nullptr;
  }

  auto texture_info = std::make_unique<TextureInfoVK>(TextureInfoVK{
      .backing_type = TextureBackingTypeVK::kAllocatedTexture,
      .allocated_texture =
          {
              .allocator = &allocator_,
              .allocation = allocation,
              .allocation_info = allocation_info,
              .image = img,
              .image_view = img_view_res.value,
          },
  });
  return std::make_shared<TextureVK>(desc, &context_, std::move(texture_info));
}

// |Allocator|
std::shared_ptr<DeviceBuffer> AllocatorVK::OnCreateBuffer(
    const DeviceBufferDescriptor& desc) {
  // TODO (kaushikiska): consider optimizing  the usage flags based on
  // StorageMode.
  auto buffer_create_info = static_cast<vk::BufferCreateInfo::NativeType>(
      vk::BufferCreateInfo()
          .setUsage(vk::BufferUsageFlagBits::eVertexBuffer |
                    vk::BufferUsageFlagBits::eIndexBuffer |
                    vk::BufferUsageFlagBits::eUniformBuffer |
                    vk::BufferUsageFlagBits::eTransferSrc |
                    vk::BufferUsageFlagBits::eTransferDst)
          .setSize(desc.size)
          .setSharingMode(vk::SharingMode::eExclusive));

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VkBuffer buffer;
  VmaAllocation buffer_allocation;
  VmaAllocationInfo buffer_allocation_info;
  auto result = vk::Result{
      vmaCreateBuffer(allocator_, &buffer_create_info, &allocCreateInfo,
                      &buffer, &buffer_allocation, &buffer_allocation_info)};

  if (result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Unable to allocate a device buffer";
    return nullptr;
  }

  auto device_allocation = std::make_unique<DeviceBufferAllocationVK>(
      allocator_, buffer, buffer_allocation, buffer_allocation_info);

  return std::make_shared<DeviceBufferVK>(desc, context_,
                                          std::move(device_allocation));
}

// |Allocator|
ISize AllocatorVK::GetMaxTextureSizeSupported() const {
  // TODO(magicianA): Get correct max texture size for Vulkan.
  // 4096 is the required limit, see below:
  // https://registry.khronos.org/vulkan/specs/1.2-extensions/html/vkspec.html#limits-minmax
  return {4096, 4096};
}
}  // namespace impeller
