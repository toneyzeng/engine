// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <string>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"

#include "flutter/impeller/runtime_stage/runtime_types.h"

namespace impeller {

class RuntimeStage {
 public:
  explicit RuntimeStage(std::shared_ptr<fml::Mapping> payload);

  ~RuntimeStage();
  RuntimeStage(RuntimeStage&&);
  RuntimeStage& operator=(RuntimeStage&&);

  bool IsValid() const;

  RuntimeShaderStage GetShaderStage() const;

  const std::vector<RuntimeUniformDescription>& GetUniforms() const;

  const std::string& GetEntrypoint() const;

  const RuntimeUniformDescription* GetUniform(const std::string& name) const;

  const std::shared_ptr<fml::Mapping>& GetCodeMapping() const;

 private:
  RuntimeShaderStage stage_ = RuntimeShaderStage::kVertex;
  std::shared_ptr<fml::Mapping> payload_;
  std::string entrypoint_;
  std::shared_ptr<fml::Mapping> code_mapping_;
  std::vector<RuntimeUniformDescription> uniforms_;
  bool is_valid_ = false;

  FML_DISALLOW_COPY_AND_ASSIGN(RuntimeStage);
};

}  // namespace impeller
