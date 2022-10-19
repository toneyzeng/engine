// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/content_context.h"

#include <sstream>

#include "impeller/entity/entity.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/formats.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/render_target.h"
#include "impeller/tessellator/tessellator.h"

namespace impeller {

void ContentContextOptions::ApplyToPipelineDescriptor(
    PipelineDescriptor& desc) const {
  auto pipeline_blend = blend_mode;
  if (blend_mode > Entity::kLastPipelineBlendMode) {
    VALIDATION_LOG << "Cannot use blend mode " << static_cast<int>(blend_mode)
                   << " as a pipeline blend.";
    pipeline_blend = BlendMode::kSourceOver;
  }

  desc.SetSampleCount(sample_count);

  ColorAttachmentDescriptor color0 = *desc.GetColorAttachmentDescriptor(0u);
  color0.alpha_blend_op = BlendOperation::kAdd;
  color0.color_blend_op = BlendOperation::kAdd;

  switch (pipeline_blend) {
    case BlendMode::kClear:
      color0.dst_alpha_blend_factor = BlendFactor::kZero;
      color0.dst_color_blend_factor = BlendFactor::kZero;
      color0.src_alpha_blend_factor = BlendFactor::kZero;
      color0.src_color_blend_factor = BlendFactor::kZero;
      break;
    case BlendMode::kSource:
      color0.dst_alpha_blend_factor = BlendFactor::kZero;
      color0.dst_color_blend_factor = BlendFactor::kZero;
      color0.src_alpha_blend_factor = BlendFactor::kOne;
      color0.src_color_blend_factor = BlendFactor::kOne;
      break;
    case BlendMode::kDestination:
      color0.dst_alpha_blend_factor = BlendFactor::kOne;
      color0.dst_color_blend_factor = BlendFactor::kOne;
      color0.src_alpha_blend_factor = BlendFactor::kZero;
      color0.src_color_blend_factor = BlendFactor::kZero;
      break;
    case BlendMode::kSourceOver:
      color0.dst_alpha_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.dst_color_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.src_alpha_blend_factor = BlendFactor::kOne;
      color0.src_color_blend_factor = BlendFactor::kOne;
      break;
    case BlendMode::kDestinationOver:
      color0.dst_alpha_blend_factor = BlendFactor::kOne;
      color0.dst_color_blend_factor = BlendFactor::kOne;
      color0.src_alpha_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      color0.src_color_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      break;
    case BlendMode::kSourceIn:
      color0.dst_alpha_blend_factor = BlendFactor::kZero;
      color0.dst_color_blend_factor = BlendFactor::kZero;
      color0.src_alpha_blend_factor = BlendFactor::kDestinationAlpha;
      color0.src_color_blend_factor = BlendFactor::kDestinationAlpha;
      break;
    case BlendMode::kDestinationIn:
      color0.dst_alpha_blend_factor = BlendFactor::kSourceAlpha;
      color0.dst_color_blend_factor = BlendFactor::kSourceAlpha;
      color0.src_alpha_blend_factor = BlendFactor::kZero;
      color0.src_color_blend_factor = BlendFactor::kZero;
      break;
    case BlendMode::kSourceOut:
      color0.dst_alpha_blend_factor = BlendFactor::kZero;
      color0.dst_color_blend_factor = BlendFactor::kZero;
      color0.src_alpha_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      color0.src_color_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      break;
    case BlendMode::kDestinationOut:
      color0.dst_alpha_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.dst_color_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.src_alpha_blend_factor = BlendFactor::kZero;
      color0.src_color_blend_factor = BlendFactor::kZero;
      break;
    case BlendMode::kSourceATop:
      color0.dst_alpha_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.dst_color_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.src_alpha_blend_factor = BlendFactor::kDestinationAlpha;
      color0.src_color_blend_factor = BlendFactor::kDestinationAlpha;
      break;
    case BlendMode::kDestinationATop:
      color0.dst_alpha_blend_factor = BlendFactor::kSourceAlpha;
      color0.dst_color_blend_factor = BlendFactor::kSourceAlpha;
      color0.src_alpha_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      color0.src_color_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      break;
    case BlendMode::kXor:
      color0.dst_alpha_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.dst_color_blend_factor = BlendFactor::kOneMinusSourceAlpha;
      color0.src_alpha_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      color0.src_color_blend_factor = BlendFactor::kOneMinusDestinationAlpha;
      break;
    case BlendMode::kPlus:
      color0.dst_alpha_blend_factor = BlendFactor::kOne;
      color0.dst_color_blend_factor = BlendFactor::kOne;
      color0.src_alpha_blend_factor = BlendFactor::kOne;
      color0.src_color_blend_factor = BlendFactor::kOne;
      break;
    case BlendMode::kModulate:
      color0.dst_alpha_blend_factor = BlendFactor::kSourceAlpha;
      color0.dst_color_blend_factor = BlendFactor::kSourceColor;
      color0.src_alpha_blend_factor = BlendFactor::kZero;
      color0.src_color_blend_factor = BlendFactor::kZero;
      break;
    default:
      FML_UNREACHABLE();
  }
  desc.SetColorAttachmentDescriptor(0u, std::move(color0));

  if (desc.GetFrontStencilAttachmentDescriptor().has_value()) {
    StencilAttachmentDescriptor stencil =
        desc.GetFrontStencilAttachmentDescriptor().value();
    stencil.stencil_compare = stencil_compare;
    stencil.depth_stencil_pass = stencil_operation;
    desc.SetStencilAttachmentDescriptors(stencil);
  }
}

template <typename PipelineT>
static std::unique_ptr<PipelineT> CreateDefaultPipeline(
    const Context& context) {
  auto desc = PipelineT::Builder::MakeDefaultPipelineDescriptor(context);
  if (!desc.has_value()) {
    return nullptr;
  }
  // Apply default ContentContextOptions to the descriptor.
  ContentContextOptions{}.ApplyToPipelineDescriptor(*desc);
  return std::make_unique<PipelineT>(context, desc);
}

ContentContext::ContentContext(std::shared_ptr<Context> context)
    : context_(std::move(context)),
      tessellator_(std::make_shared<Tessellator>()) {
  if (!context_ || !context_->IsValid()) {
    return;
  }

  solid_fill_pipelines_[{}] =
      CreateDefaultPipeline<SolidFillPipeline>(*context_);
  linear_gradient_fill_pipelines_[{}] =
      CreateDefaultPipeline<LinearGradientFillPipeline>(*context_);
  radial_gradient_fill_pipelines_[{}] =
      CreateDefaultPipeline<RadialGradientFillPipeline>(*context_);
  sweep_gradient_fill_pipelines_[{}] =
      CreateDefaultPipeline<SweepGradientFillPipeline>(*context_);
  rrect_blur_pipelines_[{}] =
      CreateDefaultPipeline<RRectBlurPipeline>(*context_);
  texture_blend_pipelines_[{}] =
      CreateDefaultPipeline<BlendPipeline>(*context_);
  blend_color_pipelines_[{}] =
      CreateDefaultPipeline<BlendColorPipeline>(*context_);
  blend_colorburn_pipelines_[{}] =
      CreateDefaultPipeline<BlendColorBurnPipeline>(*context_);
  blend_colordodge_pipelines_[{}] =
      CreateDefaultPipeline<BlendColorDodgePipeline>(*context_);
  blend_darken_pipelines_[{}] =
      CreateDefaultPipeline<BlendDarkenPipeline>(*context_);
  blend_difference_pipelines_[{}] =
      CreateDefaultPipeline<BlendDifferencePipeline>(*context_);
  blend_exclusion_pipelines_[{}] =
      CreateDefaultPipeline<BlendExclusionPipeline>(*context_);
  blend_hardlight_pipelines_[{}] =
      CreateDefaultPipeline<BlendHardLightPipeline>(*context_);
  blend_hue_pipelines_[{}] = CreateDefaultPipeline<BlendHuePipeline>(*context_);
  blend_lighten_pipelines_[{}] =
      CreateDefaultPipeline<BlendLightenPipeline>(*context_);
  blend_luminosity_pipelines_[{}] =
      CreateDefaultPipeline<BlendLuminosityPipeline>(*context_);
  blend_multiply_pipelines_[{}] =
      CreateDefaultPipeline<BlendMultiplyPipeline>(*context_);
  blend_overlay_pipelines_[{}] =
      CreateDefaultPipeline<BlendOverlayPipeline>(*context_);
  blend_saturation_pipelines_[{}] =
      CreateDefaultPipeline<BlendSaturationPipeline>(*context_);
  blend_screen_pipelines_[{}] =
      CreateDefaultPipeline<BlendScreenPipeline>(*context_);
  blend_softlight_pipelines_[{}] =
      CreateDefaultPipeline<BlendSoftLightPipeline>(*context_);
  texture_pipelines_[{}] = CreateDefaultPipeline<TexturePipeline>(*context_);
  tiled_texture_pipelines_[{}] =
      CreateDefaultPipeline<TiledTexturePipeline>(*context_);
  gaussian_blur_pipelines_[{}] =
      CreateDefaultPipeline<GaussianBlurPipeline>(*context_);
  border_mask_blur_pipelines_[{}] =
      CreateDefaultPipeline<BorderMaskBlurPipeline>(*context_);
  morphology_filter_pipelines_[{}] =
      CreateDefaultPipeline<MorphologyFilterPipeline>(*context_);
  color_matrix_color_filter_pipelines_[{}] =
      CreateDefaultPipeline<ColorMatrixColorFilterPipeline>(*context_);
  linear_to_srgb_filter_pipelines_[{}] =
      CreateDefaultPipeline<LinearToSrgbFilterPipeline>(*context_);
  srgb_to_linear_filter_pipelines_[{}] =
      CreateDefaultPipeline<SrgbToLinearFilterPipeline>(*context_);
  glyph_atlas_pipelines_[{}] =
      CreateDefaultPipeline<GlyphAtlasPipeline>(*context_);
  glyph_atlas_sdf_pipelines_[{}] =
      CreateDefaultPipeline<GlyphAtlasSdfPipeline>(*context_);
  geometry_color_pipelines_[{}] =
      CreateDefaultPipeline<GeometryColorPipeline>(*context_);
  geometry_position_pipelines_[{}] =
      CreateDefaultPipeline<GeometryPositionPipeline>(*context_);
  atlas_pipelines_[{}] = CreateDefaultPipeline<AtlasPipeline>(*context_);

  // Pipelines that are variants of the base pipelines with custom descriptors.
  // TODO(98684): Rework this API to allow fetching the descriptor without
  //              waiting for the pipeline to build.
  if (auto solid_fill_pipeline = solid_fill_pipelines_[{}]->WaitAndGet()) {
    auto clip_pipeline_descriptor = solid_fill_pipeline->GetDescriptor();
    clip_pipeline_descriptor.SetLabel("Clip Pipeline");
    // Disable write to all color attachments.
    auto color_attachments =
        clip_pipeline_descriptor.GetColorAttachmentDescriptors();
    for (auto& color_attachment : color_attachments) {
      color_attachment.second.write_mask =
          static_cast<uint64_t>(ColorWriteMask::kNone);
    }
    clip_pipeline_descriptor.SetColorAttachmentDescriptors(
        std::move(color_attachments));
    clip_pipelines_[{}] =
        std::make_unique<ClipPipeline>(*context_, clip_pipeline_descriptor);
  } else {
    return;
  }

  is_valid_ = true;
}

ContentContext::~ContentContext() = default;

bool ContentContext::IsValid() const {
  return is_valid_;
}

std::shared_ptr<Texture> ContentContext::MakeSubpass(
    ISize texture_size,
    SubpassCallback subpass_callback) const {
  auto context = GetContext();

  RenderTarget subpass_target;
  if (context->SupportsOffscreenMSAA()) {
    subpass_target = RenderTarget::CreateOffscreenMSAA(*context, texture_size);
  } else {
    subpass_target = RenderTarget::CreateOffscreen(*context, texture_size);
  }
  auto subpass_texture = subpass_target.GetRenderTargetTexture();
  if (!subpass_texture) {
    return nullptr;
  }

  auto sub_command_buffer = context->CreateCommandBuffer();
  sub_command_buffer->SetLabel("Offscreen Contents Command Buffer");
  if (!sub_command_buffer) {
    return nullptr;
  }

  auto sub_renderpass = sub_command_buffer->CreateRenderPass(subpass_target);
  if (!sub_renderpass) {
    return nullptr;
  }
  sub_renderpass->SetLabel("OffscreenContentsPass");

  if (!subpass_callback(*this, *sub_renderpass)) {
    return nullptr;
  }

  if (!sub_renderpass->EncodeCommands()) {
    return nullptr;
  }

  if (!sub_command_buffer->SubmitCommands()) {
    return nullptr;
  }

  return subpass_texture;
}

std::shared_ptr<Tessellator> ContentContext::GetTessellator() const {
  return tessellator_;
}

std::shared_ptr<Context> ContentContext::GetContext() const {
  return context_;
}

}  // namespace impeller
