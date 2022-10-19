// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "flutter/fml/concurrent_message_loop.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "impeller/base/backend_cast.h"
#include "impeller/renderer/backend/vulkan/command_pool_vk.h"
#include "impeller/renderer/backend/vulkan/descriptor_pool_vk.h"
#include "impeller/renderer/backend/vulkan/pipeline_library_vk.h"
#include "impeller/renderer/backend/vulkan/sampler_library_vk.h"
#include "impeller/renderer/backend/vulkan/shader_library_vk.h"
#include "impeller/renderer/backend/vulkan/surface_producer_vk.h"
#include "impeller/renderer/backend/vulkan/swapchain_vk.h"
#include "impeller/renderer/backend/vulkan/vk.h"
#include "impeller/renderer/context.h"

namespace impeller {

class ContextVK final : public Context, public BackendCast<ContextVK, Context> {
 public:
  static std::shared_ptr<ContextVK> Create(
      PFN_vkGetInstanceProcAddr proc_address_callback,
      const std::vector<std::shared_ptr<fml::Mapping>>& shader_libraries_data,
      const std::shared_ptr<const fml::Mapping>& pipeline_cache_data,
      std::shared_ptr<fml::ConcurrentTaskRunner> worker_task_runner,
      const std::string& label);

  // |Context|
  ~ContextVK() override;

  // |Context|
  bool IsValid() const override;

  template <typename T>
  bool SetDebugName(T handle, std::string_view label) const {
    uint64_t handle_ptr =
        reinterpret_cast<uint64_t>(static_cast<typename T::NativeType>(handle));

    std::string label_str = std::string(label);

    auto ret = device_->setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT()
            .setObjectType(T::objectType)
            .setObjectHandle(handle_ptr)
            .setPObjectName(label_str.c_str()));

    if (ret != vk::Result::eSuccess) {
      VALIDATION_LOG << "unable to set debug name";
      return false;
    }

    return true;
  }

  vk::Instance GetInstance() const;

  void SetupSwapchain(vk::UniqueSurfaceKHR surface);

  std::unique_ptr<Surface> AcquireSurface(size_t current_frame);

  std::shared_ptr<DescriptorPoolVK> GetDescriptorPool() const;

 private:
  std::shared_ptr<fml::ConcurrentTaskRunner> worker_task_runner_;
  vk::UniqueInstance instance_;
  vk::UniqueDebugUtilsMessengerEXT debug_messenger_;
  vk::PhysicalDevice physical_device_;
  vk::UniqueDevice device_;
  std::shared_ptr<Allocator> allocator_;
  std::shared_ptr<ShaderLibraryVK> shader_library_;
  std::shared_ptr<SamplerLibraryVK> sampler_library_;
  std::shared_ptr<PipelineLibraryVK> pipeline_library_;
  vk::Queue graphics_queue_;
  vk::Queue compute_queue_;
  vk::Queue transfer_queue_;
  vk::Queue present_queue_;
  vk::UniqueSurfaceKHR surface_;
  std::unique_ptr<SwapchainVK> swapchain_;
  std::unique_ptr<CommandPoolVK> graphics_command_pool_;
  std::unique_ptr<SurfaceProducerVK> surface_producer_;
  std::shared_ptr<WorkQueue> work_queue_;
  std::shared_ptr<DescriptorPoolVK> descriptor_pool_;
  bool is_valid_ = false;

  ContextVK(
      PFN_vkGetInstanceProcAddr proc_address_callback,
      const std::vector<std::shared_ptr<fml::Mapping>>& shader_libraries_data,
      const std::shared_ptr<const fml::Mapping>& pipeline_cache_data,
      std::shared_ptr<fml::ConcurrentTaskRunner> worker_task_runner,
      const std::string& label);

  // |Context|
  std::shared_ptr<Allocator> GetResourceAllocator() const override;

  // |Context|
  std::shared_ptr<ShaderLibrary> GetShaderLibrary() const override;

  // |Context|
  std::shared_ptr<SamplerLibrary> GetSamplerLibrary() const override;

  // |Context|
  std::shared_ptr<PipelineLibrary> GetPipelineLibrary() const override;

  // |Context|
  std::shared_ptr<CommandBuffer> CreateCommandBuffer() const override;

  // |Context|
  std::shared_ptr<WorkQueue> GetWorkQueue() const override;

  // |Context|
  bool SupportsOffscreenMSAA() const override;

  FML_DISALLOW_COPY_AND_ASSIGN(ContextVK);
};

}  // namespace impeller
