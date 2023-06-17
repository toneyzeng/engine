// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "flutter/fml/macros.h"
#include "impeller/base/backend_cast.h"
#include "impeller/renderer/backend/vulkan/device_holder.h"
#include "impeller/renderer/backend/vulkan/vk.h"
#include "impeller/renderer/pipeline.h"

namespace impeller {

class ComputePipelineVK final
    : public Pipeline<ComputePipelineDescriptor>,
      public BackendCast<ComputePipelineVK,
                         Pipeline<ComputePipelineDescriptor>> {
 public:
  ComputePipelineVK(std::weak_ptr<DeviceHolder> device_holder,
                    std::weak_ptr<PipelineLibrary> library,
                    const ComputePipelineDescriptor& desc,
                    vk::UniquePipeline pipeline,
                    vk::UniquePipelineLayout layout,
                    vk::UniqueDescriptorSetLayout descriptor_set_layout);

  // |Pipeline|
  ~ComputePipelineVK() override;

  const vk::Pipeline& GetPipeline() const;

  const vk::PipelineLayout& GetPipelineLayout() const;

  const vk::DescriptorSetLayout& GetDescriptorSetLayout() const;

 private:
  friend class PipelineLibraryVK;

  std::weak_ptr<DeviceHolder> device_holder_;
  vk::UniquePipeline pipeline_;
  vk::UniquePipelineLayout layout_;
  vk::UniqueDescriptorSetLayout descriptor_set_layout_;
  bool is_valid_ = false;

  // |Pipeline|
  bool IsValid() const override;

  FML_DISALLOW_COPY_AND_ASSIGN(ComputePipelineVK);
};

}  // namespace impeller
