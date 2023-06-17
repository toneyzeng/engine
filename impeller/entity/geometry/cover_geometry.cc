// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/geometry/cover_geometry.h"

#include "impeller/renderer/render_pass.h"

namespace impeller {

CoverGeometry::CoverGeometry() = default;

CoverGeometry::~CoverGeometry() = default;

GeometryResult CoverGeometry::GetPositionBuffer(const ContentContext& renderer,
                                                const Entity& entity,
                                                RenderPass& pass) {
  auto rect = Rect(Size(pass.GetRenderTargetSize()));
  constexpr uint16_t kRectIndicies[4] = {0, 1, 2, 3};
  auto& host_buffer = pass.GetTransientsBuffer();
  return GeometryResult{
      .type = PrimitiveType::kTriangleStrip,
      .vertex_buffer =
          {
              .vertex_buffer = host_buffer.Emplace(
                  rect.GetPoints().data(), 8 * sizeof(float), alignof(float)),
              .index_buffer = host_buffer.Emplace(
                  kRectIndicies, 4 * sizeof(uint16_t), alignof(uint16_t)),
              .vertex_count = 4,
              .index_type = IndexType::k16bit,
          },
      .transform = Matrix::MakeOrthographic(pass.GetRenderTargetSize()),
      .prevent_overdraw = false,
  };
}

// |Geometry|
GeometryResult CoverGeometry::GetPositionUVBuffer(
    Rect texture_coverage,
    Matrix effect_transform,
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass) {
  auto rect = Rect(Size(pass.GetRenderTargetSize()));
  return ComputeUVGeometryForRect(rect, texture_coverage, effect_transform,
                                  renderer, entity, pass);
}

GeometryVertexType CoverGeometry::GetVertexType() const {
  return GeometryVertexType::kPosition;
}

std::optional<Rect> CoverGeometry::GetCoverage(const Matrix& transform) const {
  return Rect::MakeMaximum();
}

}  // namespace impeller
