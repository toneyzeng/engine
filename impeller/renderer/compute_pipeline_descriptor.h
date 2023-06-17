// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "flutter/fml/hash_combine.h"
#include "flutter/fml/macros.h"
#include "impeller/base/comparable.h"
#include "impeller/core/formats.h"
#include "impeller/core/shader_types.h"
#include "impeller/tessellator/tessellator.h"

namespace impeller {

class ShaderFunction;
template <typename T>
class Pipeline;

class ComputePipelineDescriptor final
    : public Comparable<ComputePipelineDescriptor> {
 public:
  ComputePipelineDescriptor();

  ~ComputePipelineDescriptor();

  ComputePipelineDescriptor& SetLabel(std::string label);

  const std::string& GetLabel() const;

  ComputePipelineDescriptor& SetStageEntrypoint(
      std::shared_ptr<const ShaderFunction> function);

  std::shared_ptr<const ShaderFunction> GetStageEntrypoint() const;

  // Comparable<ComputePipelineDescriptor>
  std::size_t GetHash() const override;

  // Comparable<PipelineDescriptor>
  bool IsEqual(const ComputePipelineDescriptor& other) const override;

  template <size_t Size>
  bool RegisterDescriptorSetLayouts(
      const std::array<DescriptorSetLayout, Size>& inputs) {
    return RegisterDescriptorSetLayouts(inputs.data(), inputs.size());
  }

  bool RegisterDescriptorSetLayouts(const DescriptorSetLayout desc_set_layout[],
                                    size_t count);

  const std::vector<DescriptorSetLayout>& GetDescriptorSetLayouts() const;

 private:
  std::string label_;
  std::shared_ptr<const ShaderFunction> entrypoint_;
  std::vector<DescriptorSetLayout> descriptor_set_layouts_;
};

}  // namespace impeller
