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

namespace impeller {

class ClipContents final : public Contents {
 public:
  ClipContents();

  ~ClipContents();

  void SetPath(Path path);

  void SetClipOperation(Entity::ClipOperation clip_op);

  // |Contents|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

 private:
  Path path_;
  Entity::ClipOperation clip_op_ = Entity::ClipOperation::kIntersect;

  FML_DISALLOW_COPY_AND_ASSIGN(ClipContents);
};

class ClipRestoreContents final : public Contents {
 public:
  ClipRestoreContents();

  ~ClipRestoreContents();

  // |Contents|
  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  // |Contents|
  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

 private:
  FML_DISALLOW_COPY_AND_ASSIGN(ClipRestoreContents);
};

}  // namespace impeller
