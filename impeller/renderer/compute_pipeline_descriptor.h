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
#include "impeller/renderer/formats.h"
#include "impeller/renderer/shader_types.h"
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

 private:
  std::string label_;
  std::shared_ptr<const ShaderFunction> entrypoint_;
};

using ComputePipelineMap = std::unordered_map<
    ComputePipelineDescriptor,
    std::shared_future<std::shared_ptr<Pipeline<ComputePipelineDescriptor>>>,
    ComparableHash<ComputePipelineDescriptor>,
    ComparableEqual<ComputePipelineDescriptor>>;

}  // namespace impeller
