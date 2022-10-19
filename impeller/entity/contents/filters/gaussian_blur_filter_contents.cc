// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/filters/gaussian_blur_filter_contents.h"

#include <cmath>
#include <valarray>

#include "impeller/base/strings.h"
#include "impeller/base/validation.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/filters/filter_contents.h"
#include "impeller/geometry/rect.h"
#include "impeller/geometry/scalar.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/formats.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/render_target.h"
#include "impeller/renderer/sampler_descriptor.h"
#include "impeller/renderer/sampler_library.h"

namespace impeller {

DirectionalGaussianBlurFilterContents::DirectionalGaussianBlurFilterContents() =
    default;

DirectionalGaussianBlurFilterContents::
    ~DirectionalGaussianBlurFilterContents() = default;

void DirectionalGaussianBlurFilterContents::SetSigma(Sigma sigma) {
  blur_sigma_ = sigma;
}

void DirectionalGaussianBlurFilterContents::SetSecondarySigma(Sigma sigma) {
  secondary_blur_sigma_ = sigma;
}

void DirectionalGaussianBlurFilterContents::SetDirection(Vector2 direction) {
  blur_direction_ = direction.Normalize();
  if (blur_direction_.IsZero()) {
    blur_direction_ = Vector2(0, 1);
  }
}

void DirectionalGaussianBlurFilterContents::SetBlurStyle(BlurStyle blur_style) {
  blur_style_ = blur_style;

  switch (blur_style) {
    case FilterContents::BlurStyle::kNormal:
      src_color_factor_ = false;
      inner_blur_factor_ = true;
      outer_blur_factor_ = true;
      break;
    case FilterContents::BlurStyle::kSolid:
      src_color_factor_ = true;
      inner_blur_factor_ = false;
      outer_blur_factor_ = true;
      break;
    case FilterContents::BlurStyle::kOuter:
      src_color_factor_ = false;
      inner_blur_factor_ = false;
      outer_blur_factor_ = true;
      break;
    case FilterContents::BlurStyle::kInner:
      src_color_factor_ = false;
      inner_blur_factor_ = true;
      outer_blur_factor_ = false;
      break;
  }
}

void DirectionalGaussianBlurFilterContents::SetTileMode(
    Entity::TileMode tile_mode) {
  tile_mode_ = tile_mode;
}

void DirectionalGaussianBlurFilterContents::SetSourceOverride(
    FilterInput::Ref source_override) {
  source_override_ = source_override;
}

std::optional<Snapshot> DirectionalGaussianBlurFilterContents::RenderFilter(
    const FilterInput::Vector& inputs,
    const ContentContext& renderer,
    const Entity& entity,
    const Matrix& effect_transform,
    const Rect& coverage) const {
  using VS = GaussianBlurPipeline::VertexShader;
  using FS = GaussianBlurPipeline::FragmentShader;

  //----------------------------------------------------------------------------
  /// Handle inputs.
  ///

  if (inputs.empty()) {
    return std::nullopt;
  }

  // Input 0 snapshot.

  auto input_snapshot = inputs[0]->GetSnapshot(renderer, entity);
  if (!input_snapshot.has_value()) {
    return std::nullopt;
  }

  if (blur_sigma_.sigma < kEhCloseEnough) {
    return input_snapshot.value();  // No blur to render.
  }

  auto radius = Radius{blur_sigma_}.radius;

  auto transform = entity.GetTransformation() * effect_transform;
  auto transformed_blur_radius =
      transform.TransformDirection(blur_direction_ * radius);

  auto transformed_blur_radius_length = transformed_blur_radius.GetLength();

  // If the radius length is < .5, the shader will take at most 1 sample,
  // resulting in no blur.
  if (transformed_blur_radius_length < .5) {
    return input_snapshot.value();  // No blur to render.
  }

  // A matrix that rotates the snapshot space such that the blur direction is
  // +X.
  auto texture_rotate = Matrix::MakeRotationZ(
      transformed_blur_radius.Normalize().AngleTo({1, 0}));

  // Converts local pass space to screen space. This is just the snapshot space
  // rotated such that the blur direction is +X.
  auto pass_transform = texture_rotate * input_snapshot->transform;

  // The pass texture coverage, but rotated such that the blur is in the +X
  // direction, and expanded to include the blur radius. This is used for UV
  // projection and as a source for the pass size. Note that it doesn't matter
  // which direction the space is rotated in when grabbing the pass size.
  auto pass_texture_rect = Rect::MakeSize(input_snapshot->texture->GetSize())
                               .TransformBounds(pass_transform);
  pass_texture_rect.origin.x -= transformed_blur_radius_length;
  pass_texture_rect.size.width += transformed_blur_radius_length * 2;

  // Source override snapshot.

  auto source = source_override_ ? source_override_ : inputs[0];
  auto source_snapshot = source->GetSnapshot(renderer, entity);
  if (!source_snapshot.has_value()) {
    return std::nullopt;
  }

  // UV mapping.

  auto pass_uv_project = [&texture_rotate,
                          &pass_texture_rect](Snapshot& input) {
    auto uv_matrix = Matrix::MakeScale(1 / Vector2(input.texture->GetSize())) *
                     (texture_rotate * input.transform).Invert();
    return pass_texture_rect.GetTransformedPoints(uv_matrix);
  };

  auto input_uvs = pass_uv_project(input_snapshot.value());

  auto source_uvs = pass_uv_project(source_snapshot.value());

  //----------------------------------------------------------------------------
  /// Render to texture.
  ///

  ContentContext::SubpassCallback callback = [&](const ContentContext& renderer,
                                                 RenderPass& pass) {
    auto& host_buffer = pass.GetTransientsBuffer();

    VertexBufferBuilder<VS::PerVertexData> vtx_builder;
    vtx_builder.AddVertices({
        {Point(0, 0), input_uvs[0], source_uvs[0]},
        {Point(1, 0), input_uvs[1], source_uvs[1]},
        {Point(1, 1), input_uvs[3], source_uvs[3]},
        {Point(0, 0), input_uvs[0], source_uvs[0]},
        {Point(1, 1), input_uvs[3], source_uvs[3]},
        {Point(0, 1), input_uvs[2], source_uvs[2]},
    });
    auto vtx_buffer = vtx_builder.CreateVertexBuffer(host_buffer);

    VS::FrameInfo frame_info;
    frame_info.mvp = Matrix::MakeOrthographic(ISize(1, 1));

    FS::FragInfo frag_info;
    frag_info.texture_sampler_y_coord_scale =
        input_snapshot->texture->GetYCoordScale();
    frag_info.alpha_mask_sampler_y_coord_scale =
        source_snapshot->texture->GetYCoordScale();

    auto r = Radius{transformed_blur_radius_length};
    frag_info.blur_sigma = Sigma{r}.sigma;
    frag_info.blur_radius = r.radius;

    // The blur direction is in input UV space.
    frag_info.blur_direction =
        pass_transform.Invert().TransformDirection(Vector2(1, 0)).Normalize();

    frag_info.tile_mode = static_cast<Scalar>(tile_mode_);
    frag_info.src_factor = src_color_factor_;
    frag_info.inner_blur_factor = inner_blur_factor_;
    frag_info.outer_blur_factor = outer_blur_factor_;
    frag_info.texture_size = Point(input_snapshot->GetCoverage().value().size);

    Command cmd;
    cmd.label = SPrintF("Gaussian Blur Filter (Radius=%.2f)",
                        transformed_blur_radius_length);
    auto options = OptionsFromPass(pass);
    options.blend_mode = BlendMode::kSource;
    cmd.pipeline = renderer.GetGaussianBlurPipeline(options);
    cmd.BindVertices(vtx_buffer);

    FS::BindTextureSampler(
        cmd, input_snapshot->texture,
        renderer.GetContext()->GetSamplerLibrary()->GetSampler(
            input_snapshot->sampler_descriptor));
    FS::BindAlphaMaskSampler(
        cmd, source_snapshot->texture,
        renderer.GetContext()->GetSamplerLibrary()->GetSampler(
            source_snapshot->sampler_descriptor));
    VS::BindFrameInfo(cmd, host_buffer.EmplaceUniform(frame_info));
    FS::BindFragInfo(cmd, host_buffer.EmplaceUniform(frag_info));

    return pass.AddCommand(cmd);
  };

  Vector2 scale;
  auto scale_curve = [](Scalar radius) {
    const Scalar d = 4.0;
    return std::min(1.0, d / (std::max(1.0f, radius) + d - 1.0));
  };
  {
    scale.x = scale_curve(transformed_blur_radius_length);

    Scalar y_radius = std::abs(pass_transform.GetDirectionScale(Vector2(
        0, source_override_ ? Radius{secondary_blur_sigma_}.radius : 1)));
    scale.y = scale_curve(y_radius);
  }

  Vector2 scaled_size = pass_texture_rect.size * scale;
  ISize floored_size = ISize(scaled_size.x, scaled_size.y);

  auto out_texture = renderer.MakeSubpass(floored_size, callback);

  if (!out_texture) {
    return std::nullopt;
  }
  out_texture->SetLabel("DirectionalGaussianBlurFilter Texture");

  SamplerDescriptor sampler_desc;
  sampler_desc.min_filter = MinMagFilter::kLinear;
  sampler_desc.mag_filter = MinMagFilter::kLinear;

  return Snapshot{
      .texture = out_texture,
      .transform =
          texture_rotate.Invert() *
          Matrix::MakeTranslation(pass_texture_rect.origin) *
          Matrix::MakeScale((1 / scale) * (scaled_size / floored_size)),
      .sampler_descriptor = sampler_desc,
      .opacity = input_snapshot->opacity};
}

std::optional<Rect> DirectionalGaussianBlurFilterContents::GetFilterCoverage(
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

  auto transform = inputs[0]->GetTransform(entity) * effect_transform;
  auto transformed_blur_vector =
      transform.TransformDirection(blur_direction_ * Radius{blur_sigma_}.radius)
          .Abs();
  auto extent = coverage->size + transformed_blur_vector * 2;
  return Rect(coverage->origin - transformed_blur_vector,
              Size(extent.x, extent.y));
}

}  // namespace impeller
