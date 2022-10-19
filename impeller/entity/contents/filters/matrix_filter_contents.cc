// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/filters/matrix_filter_contents.h"

namespace impeller {

MatrixFilterContents::MatrixFilterContents() = default;

MatrixFilterContents::~MatrixFilterContents() = default;

void MatrixFilterContents::SetMatrix(Matrix matrix) {
  matrix_ = matrix;
}

void MatrixFilterContents::SetSamplerDescriptor(SamplerDescriptor desc) {
  sampler_descriptor_ = std::move(desc);
}

std::optional<Snapshot> MatrixFilterContents::RenderFilter(
    const FilterInput::Vector& inputs,
    const ContentContext& renderer,
    const Entity& entity,
    const Matrix& effect_transform,
    const Rect& coverage) const {
  auto snapshot = inputs[0]->GetSnapshot(renderer, entity);
  if (!snapshot.has_value()) {
    return std::nullopt;
  }

  snapshot->transform = entity.GetTransformation() *           //
                        matrix_ *                              //
                        entity.GetTransformation().Invert() *  //
                        snapshot->transform;
  snapshot->sampler_descriptor = sampler_descriptor_;
  return snapshot;
}

std::optional<Rect> MatrixFilterContents::GetFilterCoverage(
    const FilterInput::Vector& inputs,
    const Entity& entity,
    const Matrix& effect_transform) const {
  if (inputs.empty()) {
    return std::nullopt;
  }

  auto coverage = inputs[0]->GetCoverage(entity);
  if (!coverage.has_value()) {
    return std::nullopt;
  }

  auto transform = inputs[0]->GetTransform(entity) *  //
                   matrix_ *                          //
                   inputs[0]->GetTransform(entity).Invert();
  return coverage->TransformBounds(transform);
}

}  // namespace impeller
