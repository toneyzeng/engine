// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "flutter/fml/macros.h"
#include "impeller/entity/contents/contents.h"
#include "impeller/entity/entity.h"
#include "impeller/renderer/sampler_descriptor.h"

namespace impeller {

class AtlasContents final : public Contents {
 public:
  explicit AtlasContents();

  ~AtlasContents() override;

  void SetTexture(std::shared_ptr<Texture> texture);

  std::shared_ptr<Texture> GetTexture() const;

  void SetTransforms(std::vector<Matrix> transforms);

  void SetBlendMode(BlendMode blend_mode);

  void SetTextureCoordinates(std::vector<Rect> texture_coords);

  void SetColors(std::vector<Color> colors);

  void SetCullRect(std::optional<Rect> cull_rect);

  void SetSamplerDescriptor(SamplerDescriptor desc);

  void SetAlpha(Scalar alpha);

  const SamplerDescriptor& GetSamplerDescriptor() const;

  // |Contents|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

 private:
  std::shared_ptr<Texture> texture_;
  std::vector<Rect> texture_coords_;
  std::vector<Color> colors_;
  std::vector<Matrix> transforms_;
  BlendMode blend_mode_;
  std::optional<Rect> cull_rect_;
  Scalar alpha_ = 1.0;
  SamplerDescriptor sampler_descriptor_ = {};

  FML_DISALLOW_COPY_AND_ASSIGN(AtlasContents);
};

}  // namespace impeller
