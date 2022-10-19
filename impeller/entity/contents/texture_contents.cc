// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "texture_contents.h"

#include <memory>
#include <optional>

#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/texture_fill.frag.h"
#include "impeller/entity/texture_fill.vert.h"
#include "impeller/geometry/constants.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/renderer/formats.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/sampler_library.h"
#include "impeller/tessellator/tessellator.h"

namespace impeller {

TextureContents::TextureContents() = default;

TextureContents::~TextureContents() = default;

std::shared_ptr<TextureContents> TextureContents::MakeRect(Rect destination) {
  auto contents = std::make_shared<TextureContents>();
  contents->path_ = PathBuilder{}.AddRect(destination).TakePath();
  contents->is_rect_ = true;
  return contents;
}

void TextureContents::SetLabel(std::string label) {
  label_ = label;
}

void TextureContents::SetPath(Path path) {
  path_ = std::move(path);
  is_rect_ = false;
}

void TextureContents::SetTexture(std::shared_ptr<Texture> texture) {
  texture_ = std::move(texture);
}

std::shared_ptr<Texture> TextureContents::GetTexture() const {
  return texture_;
}

void TextureContents::SetOpacity(Scalar opacity) {
  opacity_ = opacity;
}

void TextureContents::SetStencilEnabled(bool enabled) {
  stencil_enabled_ = enabled;
}

std::optional<Rect> TextureContents::GetCoverage(const Entity& entity) const {
  if (opacity_ == 0) {
    return std::nullopt;
  }
  return path_.GetTransformedBoundingBox(entity.GetTransformation());
};

std::optional<Snapshot> TextureContents::RenderToSnapshot(
    const ContentContext& renderer,
    const Entity& entity) const {
  auto bounds = path_.GetBoundingBox();
  if (!bounds.has_value()) {
    return std::nullopt;
  }

  // Passthrough textures that have simple rectangle paths and complete source
  // rects.
  if (is_rect_ && source_rect_ == Rect::MakeSize(texture_->GetSize()) &&
      (opacity_ >= 1 - kEhCloseEnough || defer_applying_opacity_)) {
    auto scale = Vector2(bounds->size / Size(texture_->GetSize()));
    return Snapshot{.texture = texture_,
                    .transform = entity.GetTransformation() *
                                 Matrix::MakeTranslation(bounds->origin) *
                                 Matrix::MakeScale(scale),
                    .sampler_descriptor = sampler_descriptor_,
                    .opacity = opacity_};
  }
  return Contents::RenderToSnapshot(renderer, entity);
}

bool TextureContents::Render(const ContentContext& renderer,
                             const Entity& entity,
                             RenderPass& pass) const {
  if (texture_ == nullptr) {
    return true;
  }

  using VS = TextureFillVertexShader;
  using FS = TextureFillFragmentShader;

  const auto coverage_rect = path_.GetBoundingBox();

  if (!coverage_rect.has_value()) {
    return true;
  }

  if (coverage_rect->size.IsEmpty()) {
    return true;
  }

  const auto texture_size = texture_->GetSize();
  if (texture_size.IsEmpty()) {
    return true;
  }

  if (source_rect_.IsEmpty()) {
    return true;
  }

  VertexBufferBuilder<VS::PerVertexData> vertex_builder;
  {
    const auto tess_result = renderer.GetTessellator()->Tessellate(
        path_.GetFillType(), path_.CreatePolyline(),
        [this, &vertex_builder, &coverage_rect, &texture_size](
            const float* vertices, size_t vertices_size,
            const uint16_t* indices, size_t indices_size) {
          for (auto i = 0u; i < vertices_size; i++) {
            VS::PerVertexData data;
            Point vtx = {vertices[i], vertices[i + 1]};
            data.position = vtx;
            auto coverage_coords =
                (vtx - coverage_rect->origin) / coverage_rect->size;
            data.texture_coords =
                (source_rect_.origin + source_rect_.size * coverage_coords) /
                texture_size;
            vertex_builder.AppendVertex(data);
          }
          for (auto i = 0u; i < indices_size; i++) {
            vertex_builder.AppendIndex(indices[i]);
          }
          return true;
        });

    if (tess_result == Tessellator::Result::kInputError) {
      return true;
    }
    if (tess_result == Tessellator::Result::kTessellationError) {
      return false;
    }
  }

  if (!vertex_builder.HasVertices()) {
    return true;
  }

  auto& host_buffer = pass.GetTransientsBuffer();

  VS::VertInfo vert_info;
  vert_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                  entity.GetTransformation();

  FS::FragInfo frag_info;
  frag_info.texture_sampler_y_coord_scale = texture_->GetYCoordScale();
  frag_info.alpha = opacity_;

  Command cmd;
  cmd.label = "Texture Fill";
  if (!label_.empty()) {
    cmd.label += ": " + label_;
  }

  auto pipeline_options = OptionsFromPassAndEntity(pass, entity);
  if (!stencil_enabled_) {
    pipeline_options.stencil_compare = CompareFunction::kAlways;
  }
  cmd.pipeline = renderer.GetTexturePipeline(pipeline_options);
  cmd.stencil_reference = entity.GetStencilDepth();
  cmd.BindVertices(vertex_builder.CreateVertexBuffer(host_buffer));
  VS::BindVertInfo(cmd, host_buffer.EmplaceUniform(vert_info));
  FS::BindFragInfo(cmd, host_buffer.EmplaceUniform(frag_info));
  FS::BindTextureSampler(cmd, texture_,
                         renderer.GetContext()->GetSamplerLibrary()->GetSampler(
                             sampler_descriptor_));
  pass.AddCommand(std::move(cmd));

  return true;
}

void TextureContents::SetSourceRect(const Rect& source_rect) {
  source_rect_ = source_rect;
}

const Rect& TextureContents::GetSourceRect() const {
  return source_rect_;
}

void TextureContents::SetSamplerDescriptor(SamplerDescriptor desc) {
  sampler_descriptor_ = std::move(desc);
}

const SamplerDescriptor& TextureContents::GetSamplerDescriptor() const {
  return sampler_descriptor_;
}

void TextureContents::SetDeferApplyingOpacity(bool defer_applying_opacity) {
  defer_applying_opacity_ = defer_applying_opacity;
}

}  // namespace impeller
