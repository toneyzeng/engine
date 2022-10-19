// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/aiks/canvas.h"

#include <algorithm>
#include <optional>

#include "flutter/fml/logging.h"
#include "impeller/aiks/paint_pass_delegate.h"
#include "impeller/entity/contents/atlas_contents.h"
#include "impeller/entity/contents/clip_contents.h"
#include "impeller/entity/contents/rrect_shadow_contents.h"
#include "impeller/entity/contents/text_contents.h"
#include "impeller/entity/contents/texture_contents.h"
#include "impeller/entity/contents/vertices_contents.h"
#include "impeller/entity/geometry.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/geometry/vertices.h"

namespace impeller {

Canvas::Canvas() {
  Initialize();
}

Canvas::~Canvas() = default;

void Canvas::Initialize() {
  base_pass_ = std::make_unique<EntityPass>();
  current_pass_ = base_pass_.get();
  xformation_stack_.emplace_back(CanvasStackEntry{});
  FML_DCHECK(GetSaveCount() == 1u);
  FML_DCHECK(base_pass_->GetSubpassesDepth() == 1u);
}

void Canvas::Reset() {
  base_pass_ = nullptr;
  current_pass_ = nullptr;
  xformation_stack_ = {};
}

void Canvas::Save() {
  Save(false);
}

void Canvas::Save(
    bool create_subpass,
    BlendMode blend_mode,
    std::optional<EntityPass::BackdropFilterProc> backdrop_filter) {
  auto entry = CanvasStackEntry{};
  entry.xformation = xformation_stack_.back().xformation;
  entry.stencil_depth = xformation_stack_.back().stencil_depth;
  if (create_subpass) {
    entry.is_subpass = true;
    auto subpass = std::make_unique<EntityPass>();
    subpass->SetBackdropFilter(backdrop_filter);
    subpass->SetBlendMode(blend_mode);
    current_pass_ = GetCurrentPass().AddSubpass(std::move(subpass));
    current_pass_->SetTransformation(xformation_stack_.back().xformation);
    current_pass_->SetStencilDepth(xformation_stack_.back().stencil_depth);
  }
  xformation_stack_.emplace_back(std::move(entry));
}

bool Canvas::Restore() {
  FML_DCHECK(xformation_stack_.size() > 0);
  if (xformation_stack_.size() == 1) {
    return false;
  }
  if (xformation_stack_.back().is_subpass) {
    current_pass_ = GetCurrentPass().GetSuperpass();
    FML_DCHECK(current_pass_);
  }

  bool contains_clips = xformation_stack_.back().contains_clips;
  xformation_stack_.pop_back();

  if (contains_clips) {
    RestoreClip();
  }

  return true;
}

void Canvas::Concat(const Matrix& xformation) {
  xformation_stack_.back().xformation = GetCurrentTransformation() * xformation;
}

void Canvas::PreConcat(const Matrix& xformation) {
  xformation_stack_.back().xformation = xformation * GetCurrentTransformation();
}

void Canvas::ResetTransform() {
  xformation_stack_.back().xformation = {};
}

void Canvas::Transform(const Matrix& xformation) {
  Concat(xformation);
}

const Matrix& Canvas::GetCurrentTransformation() const {
  return xformation_stack_.back().xformation;
}

void Canvas::Translate(const Vector3& offset) {
  Concat(Matrix::MakeTranslation(offset));
}

void Canvas::Scale(const Vector2& scale) {
  Concat(Matrix::MakeScale(scale));
}

void Canvas::Scale(const Vector3& scale) {
  Concat(Matrix::MakeScale(scale));
}

void Canvas::Skew(Scalar sx, Scalar sy) {
  Concat(Matrix::MakeSkew(sx, sy));
}

void Canvas::Rotate(Radians radians) {
  Concat(Matrix::MakeRotationZ(radians));
}

size_t Canvas::GetSaveCount() const {
  return xformation_stack_.size();
}

void Canvas::RestoreToCount(size_t count) {
  while (GetSaveCount() > count) {
    if (!Restore()) {
      return;
    }
  }
}

void Canvas::DrawPath(Path path, Paint paint) {
  Entity entity;
  entity.SetTransformation(GetCurrentTransformation());
  entity.SetStencilDepth(GetStencilDepth());
  entity.SetBlendMode(paint.blend_mode);
  entity.SetContents(
      paint.WithFilters(paint.CreateContentsForEntity(std::move(path))));

  GetCurrentPass().AddEntity(std::move(entity));
}

void Canvas::DrawPaint(Paint paint) {
  Entity entity;
  entity.SetTransformation(GetCurrentTransformation());
  entity.SetStencilDepth(GetStencilDepth());
  entity.SetBlendMode(paint.blend_mode);
  entity.SetContents(paint.CreateContentsForEntity({}, true));

  GetCurrentPass().AddEntity(std::move(entity));
}

bool Canvas::AttemptDrawBlurredRRect(const Rect& rect,
                                     Scalar corner_radius,
                                     Paint& paint) {
  if (!paint.mask_blur_descriptor.has_value() ||
      paint.mask_blur_descriptor->style != FilterContents::BlurStyle::kNormal ||
      paint.style != Paint::Style::kFill) {
    return false;
  }

  // For symmetrically mask blurred solid RRects, absorb the mask blur and use
  // a faster SDF approximation.

  auto contents = std::make_shared<RRectShadowContents>();
  contents->SetColor(paint.color);
  contents->SetSigma(paint.mask_blur_descriptor->sigma);
  contents->SetRRect(rect, corner_radius);

  paint.mask_blur_descriptor = std::nullopt;

  Entity entity;
  entity.SetTransformation(GetCurrentTransformation());
  entity.SetStencilDepth(GetStencilDepth());
  entity.SetBlendMode(paint.blend_mode);
  entity.SetContents(paint.WithFilters(std::move(contents)));

  GetCurrentPass().AddEntity(std::move(entity));

  return true;
}

void Canvas::DrawRect(Rect rect, Paint paint) {
  if (AttemptDrawBlurredRRect(rect, 0, paint)) {
    return;
  }
  DrawPath(PathBuilder{}.AddRect(rect).TakePath(), std::move(paint));
}

void Canvas::DrawRRect(Rect rect, Scalar corner_radius, Paint paint) {
  if (AttemptDrawBlurredRRect(rect, corner_radius, paint)) {
    return;
  }
  DrawPath(PathBuilder{}.AddRoundedRect(rect, corner_radius).TakePath(),
           std::move(paint));
}

void Canvas::DrawCircle(Point center, Scalar radius, Paint paint) {
  DrawPath(PathBuilder{}.AddCircle(center, radius).TakePath(),
           std::move(paint));
}

void Canvas::ClipPath(Path path, Entity::ClipOperation clip_op) {
  auto contents = std::make_shared<ClipContents>();
  contents->SetGeometry(Geometry::MakePath(std::move(path)));
  contents->SetClipOperation(clip_op);

  Entity entity;
  entity.SetTransformation(GetCurrentTransformation());
  entity.SetContents(std::move(contents));
  entity.SetStencilDepth(GetStencilDepth());

  GetCurrentPass().AddEntity(std::move(entity));

  ++xformation_stack_.back().stencil_depth;
  xformation_stack_.back().contains_clips = true;
}

void Canvas::RestoreClip() {
  Entity entity;
  entity.SetTransformation(GetCurrentTransformation());
  // This path is empty because ClipRestoreContents just generates a quad that
  // takes up the full render target.
  entity.SetContents(std::make_shared<ClipRestoreContents>());
  entity.SetStencilDepth(GetStencilDepth());

  GetCurrentPass().AddEntity(std::move(entity));
}

void Canvas::DrawPicture(Picture picture) {
  if (!picture.pass) {
    return;
  }
  // Clone the base pass and account for the CTM updates.
  auto pass = picture.pass->Clone();
  pass->IterateAllEntities([&](auto& entity) -> bool {
    entity.IncrementStencilDepth(GetStencilDepth());
    entity.SetTransformation(GetCurrentTransformation() *
                             entity.GetTransformation());
    return true;
  });
  return;
}

void Canvas::DrawImage(std::shared_ptr<Image> image,
                       Point offset,
                       Paint paint,
                       SamplerDescriptor sampler) {
  if (!image) {
    return;
  }

  const auto source = Rect::MakeSize(image->GetSize());
  const auto dest =
      Rect::MakeXYWH(offset.x, offset.y, source.size.width, source.size.height);

  DrawImageRect(image, source, dest, std::move(paint), std::move(sampler));
}

void Canvas::DrawImageRect(std::shared_ptr<Image> image,
                           Rect source,
                           Rect dest,
                           Paint paint,
                           SamplerDescriptor sampler) {
  if (!image || source.size.IsEmpty() || dest.size.IsEmpty()) {
    return;
  }

  auto size = image->GetSize();

  if (size.IsEmpty()) {
    return;
  }

  auto contents = TextureContents::MakeRect(dest);
  contents->SetTexture(image->GetTexture());
  contents->SetSourceRect(source);
  contents->SetSamplerDescriptor(std::move(sampler));
  contents->SetOpacity(paint.color.alpha);

  Entity entity;
  entity.SetBlendMode(paint.blend_mode);
  entity.SetStencilDepth(GetStencilDepth());
  entity.SetContents(paint.WithFilters(contents, false));
  entity.SetTransformation(GetCurrentTransformation());

  GetCurrentPass().AddEntity(std::move(entity));
}

Picture Canvas::EndRecordingAsPicture() {
  Picture picture;
  picture.pass = std::move(base_pass_);

  Reset();
  Initialize();

  return picture;
}

EntityPass& Canvas::GetCurrentPass() {
  FML_DCHECK(current_pass_ != nullptr);
  return *current_pass_;
}

size_t Canvas::GetStencilDepth() const {
  return xformation_stack_.back().stencil_depth;
}

void Canvas::SaveLayer(Paint paint,
                       std::optional<Rect> bounds,
                       std::optional<Paint::ImageFilterProc> backdrop_filter) {
  Save(true, paint.blend_mode, backdrop_filter);

  auto& new_layer_pass = GetCurrentPass();
  new_layer_pass.SetDelegate(
      std::make_unique<PaintPassDelegate>(paint, bounds));

  if (bounds.has_value() && !backdrop_filter.has_value()) {
    // Render target switches due to a save layer can be elided. In such cases
    // where passes are collapsed into their parent, the clipping effect to
    // the size of the render target that would have been allocated will be
    // absent. Explicitly add back a clip to reproduce that behavior. Since
    // clips never require a render target switch, this is a cheap operation.
    ClipPath(PathBuilder{}.AddRect(bounds.value()).TakePath());
  }
}

void Canvas::DrawTextFrame(TextFrame text_frame, Point position, Paint paint) {
  auto lazy_glyph_atlas = GetCurrentPass().GetLazyGlyphAtlas();

  lazy_glyph_atlas->AddTextFrame(text_frame);

  auto text_contents = std::make_shared<TextContents>();
  text_contents->SetTextFrame(std::move(text_frame));
  text_contents->SetGlyphAtlas(std::move(lazy_glyph_atlas));
  text_contents->SetColor(paint.color);

  Entity entity;
  entity.SetTransformation(GetCurrentTransformation() *
                           Matrix::MakeTranslation(position));
  entity.SetStencilDepth(GetStencilDepth());
  entity.SetBlendMode(paint.blend_mode);
  entity.SetContents(paint.WithFilters(std::move(text_contents), true));

  GetCurrentPass().AddEntity(std::move(entity));
}

void Canvas::DrawVertices(Vertices vertices,
                          BlendMode blend_mode,
                          Paint paint) {
  std::shared_ptr<VerticesContents> contents =
      std::make_shared<VerticesContents>();
  contents->SetColor(paint.color);
  contents->SetBlendMode(blend_mode);
  contents->SetGeometry(Geometry::MakeVertices(std::move(vertices)));
  Entity entity;
  entity.SetTransformation(GetCurrentTransformation());
  entity.SetStencilDepth(GetStencilDepth());
  entity.SetBlendMode(paint.blend_mode);
  entity.SetContents(paint.WithFilters(std::move(contents), true));

  GetCurrentPass().AddEntity(std::move(entity));
}

void Canvas::DrawAtlas(std::shared_ptr<Image> atlas,
                       std::vector<Matrix> transforms,
                       std::vector<Rect> texture_coordinates,
                       std::vector<Color> colors,
                       BlendMode blend_mode,
                       SamplerDescriptor sampler,
                       std::optional<Rect> cull_rect,
                       Paint paint) {
  if (!atlas) {
    return;
  }
  auto size = atlas->GetSize();

  if (size.IsEmpty()) {
    return;
  }

  std::shared_ptr<AtlasContents> contents = std::make_shared<AtlasContents>();
  contents->SetColors(std::move(colors));
  contents->SetTransforms(std::move(transforms));
  contents->SetTextureCoordinates(std::move(texture_coordinates));
  contents->SetTexture(atlas->GetTexture());
  contents->SetSamplerDescriptor(std::move(sampler));
  contents->SetBlendMode(blend_mode);
  contents->SetCullRect(cull_rect);
  contents->SetAlpha(paint.color.alpha);

  Entity entity;
  entity.SetTransformation(GetCurrentTransformation());
  entity.SetStencilDepth(GetStencilDepth());
  entity.SetBlendMode(paint.blend_mode);
  entity.SetContents(paint.WithFilters(contents, false));

  GetCurrentPass().AddEntity(std::move(entity));
}

}  // namespace impeller
