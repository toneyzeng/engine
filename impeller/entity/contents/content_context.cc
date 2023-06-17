// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/content_context.h"

#include <memory>
#include <sstream>

#include "impeller/base/strings.h"
#include "impeller/core/formats.h"
#include "impeller/entity/entity.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/pipeline_library.h"
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
  color0.format = color_attachment_pixel_format;
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
  desc.SetColorAttachmentDescriptor(0u, color0);

  if (!has_stencil_attachment) {
    desc.ClearStencilAttachments();
  }

  auto maybe_stencil = desc.GetFrontStencilAttachmentDescriptor();
  if (maybe_stencil.has_value()) {
    StencilAttachmentDescriptor stencil = maybe_stencil.value();
    stencil.stencil_compare = stencil_compare;
    stencil.depth_stencil_pass = stencil_operation;
    desc.SetStencilAttachmentDescriptors(stencil);
  }

  desc.SetPrimitiveType(primitive_type);

  desc.SetPolygonMode(wireframe ? PolygonMode::kLine : PolygonMode::kFill);
}

template <typename PipelineT>
static std::unique_ptr<PipelineT> CreateDefaultPipeline(
    const Context& context) {
  auto desc = PipelineT::Builder::MakeDefaultPipelineDescriptor(context);
  if (!desc.has_value()) {
    return nullptr;
  }
  // Apply default ContentContextOptions to the descriptor.
  const auto default_color_fmt =
      context.GetCapabilities()->GetDefaultColorFormat();
  ContentContextOptions{.color_attachment_pixel_format = default_color_fmt}
      .ApplyToPipelineDescriptor(*desc);
  return std::make_unique<PipelineT>(context, desc);
}

ContentContext::ContentContext(std::shared_ptr<Context> context)
    : context_(std::move(context)),
      tessellator_(std::make_shared<Tessellator>()),
      alpha_glyph_atlas_context_(std::make_shared<GlyphAtlasContext>()),
      color_glyph_atlas_context_(std::make_shared<GlyphAtlasContext>()),
      scene_context_(std::make_shared<scene::SceneContext>(context_)) {
  if (!context_ || !context_->IsValid()) {
    return;
  }
  auto default_options = ContentContextOptions{
      .color_attachment_pixel_format =
          context_->GetCapabilities()->GetDefaultColorFormat()};

#ifdef IMPELLER_DEBUG
  checkerboard_pipelines_[default_options] =
      CreateDefaultPipeline<CheckerboardPipeline>(*context_);
#endif  // IMPELLER_DEBUG

  solid_fill_pipelines_[default_options] =
      CreateDefaultPipeline<SolidFillPipeline>(*context_);

  if (context_->GetCapabilities()->SupportsSSBO()) {
    linear_gradient_ssbo_fill_pipelines_[default_options] =
        CreateDefaultPipeline<LinearGradientSSBOFillPipeline>(*context_);
    radial_gradient_ssbo_fill_pipelines_[default_options] =
        CreateDefaultPipeline<RadialGradientSSBOFillPipeline>(*context_);
    conical_gradient_ssbo_fill_pipelines_[default_options] =
        CreateDefaultPipeline<ConicalGradientSSBOFillPipeline>(*context_);
    sweep_gradient_ssbo_fill_pipelines_[default_options] =
        CreateDefaultPipeline<SweepGradientSSBOFillPipeline>(*context_);
  } else {
    linear_gradient_fill_pipelines_[default_options] =
        CreateDefaultPipeline<LinearGradientFillPipeline>(*context_);
    radial_gradient_fill_pipelines_[default_options] =
        CreateDefaultPipeline<RadialGradientFillPipeline>(*context_);
    conical_gradient_fill_pipelines_[default_options] =
        CreateDefaultPipeline<ConicalGradientFillPipeline>(*context_);
    sweep_gradient_fill_pipelines_[default_options] =
        CreateDefaultPipeline<SweepGradientFillPipeline>(*context_);
  }

  if (context_->GetCapabilities()->SupportsFramebufferFetch()) {
    framebuffer_blend_color_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendColorPipeline>(*context_);
    framebuffer_blend_colorburn_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendColorBurnPipeline>(*context_);
    framebuffer_blend_colordodge_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendColorDodgePipeline>(*context_);
    framebuffer_blend_darken_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendDarkenPipeline>(*context_);
    framebuffer_blend_difference_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendDifferencePipeline>(*context_);
    framebuffer_blend_exclusion_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendExclusionPipeline>(*context_);
    framebuffer_blend_hardlight_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendHardLightPipeline>(*context_);
    framebuffer_blend_hue_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendHuePipeline>(*context_);
    framebuffer_blend_lighten_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendLightenPipeline>(*context_);
    framebuffer_blend_luminosity_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendLuminosityPipeline>(*context_);
    framebuffer_blend_multiply_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendMultiplyPipeline>(*context_);
    framebuffer_blend_overlay_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendOverlayPipeline>(*context_);
    framebuffer_blend_saturation_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendSaturationPipeline>(*context_);
    framebuffer_blend_screen_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendScreenPipeline>(*context_);
    framebuffer_blend_softlight_pipelines_[default_options] =
        CreateDefaultPipeline<FramebufferBlendSoftLightPipeline>(*context_);
  }

  blend_color_pipelines_[default_options] =
      CreateDefaultPipeline<BlendColorPipeline>(*context_);
  blend_colorburn_pipelines_[default_options] =
      CreateDefaultPipeline<BlendColorBurnPipeline>(*context_);
  blend_colordodge_pipelines_[default_options] =
      CreateDefaultPipeline<BlendColorDodgePipeline>(*context_);
  blend_darken_pipelines_[default_options] =
      CreateDefaultPipeline<BlendDarkenPipeline>(*context_);
  blend_difference_pipelines_[default_options] =
      CreateDefaultPipeline<BlendDifferencePipeline>(*context_);
  blend_exclusion_pipelines_[default_options] =
      CreateDefaultPipeline<BlendExclusionPipeline>(*context_);
  blend_hardlight_pipelines_[default_options] =
      CreateDefaultPipeline<BlendHardLightPipeline>(*context_);
  blend_hue_pipelines_[default_options] =
      CreateDefaultPipeline<BlendHuePipeline>(*context_);
  blend_lighten_pipelines_[default_options] =
      CreateDefaultPipeline<BlendLightenPipeline>(*context_);
  blend_luminosity_pipelines_[default_options] =
      CreateDefaultPipeline<BlendLuminosityPipeline>(*context_);
  blend_multiply_pipelines_[default_options] =
      CreateDefaultPipeline<BlendMultiplyPipeline>(*context_);
  blend_overlay_pipelines_[default_options] =
      CreateDefaultPipeline<BlendOverlayPipeline>(*context_);
  blend_saturation_pipelines_[default_options] =
      CreateDefaultPipeline<BlendSaturationPipeline>(*context_);
  blend_screen_pipelines_[default_options] =
      CreateDefaultPipeline<BlendScreenPipeline>(*context_);
  blend_softlight_pipelines_[default_options] =
      CreateDefaultPipeline<BlendSoftLightPipeline>(*context_);

  rrect_blur_pipelines_[default_options] =
      CreateDefaultPipeline<RRectBlurPipeline>(*context_);
  texture_blend_pipelines_[default_options] =
      CreateDefaultPipeline<BlendPipeline>(*context_);
  texture_pipelines_[default_options] =
      CreateDefaultPipeline<TexturePipeline>(*context_);
  position_uv_pipelines_[default_options] =
      CreateDefaultPipeline<PositionUVPipeline>(*context_);
  tiled_texture_pipelines_[default_options] =
      CreateDefaultPipeline<TiledTexturePipeline>(*context_);
  gaussian_blur_alpha_decal_pipelines_[default_options] =
      CreateDefaultPipeline<GaussianBlurAlphaDecalPipeline>(*context_);
  gaussian_blur_alpha_nodecal_pipelines_[default_options] =
      CreateDefaultPipeline<GaussianBlurAlphaPipeline>(*context_);
  gaussian_blur_noalpha_decal_pipelines_[default_options] =
      CreateDefaultPipeline<GaussianBlurDecalPipeline>(*context_);
  gaussian_blur_noalpha_nodecal_pipelines_[default_options] =
      CreateDefaultPipeline<GaussianBlurPipeline>(*context_);
  border_mask_blur_pipelines_[default_options] =
      CreateDefaultPipeline<BorderMaskBlurPipeline>(*context_);
  morphology_filter_pipelines_[default_options] =
      CreateDefaultPipeline<MorphologyFilterPipeline>(*context_);
  color_matrix_color_filter_pipelines_[default_options] =
      CreateDefaultPipeline<ColorMatrixColorFilterPipeline>(*context_);
  linear_to_srgb_filter_pipelines_[default_options] =
      CreateDefaultPipeline<LinearToSrgbFilterPipeline>(*context_);
  srgb_to_linear_filter_pipelines_[default_options] =
      CreateDefaultPipeline<SrgbToLinearFilterPipeline>(*context_);
  glyph_atlas_pipelines_[default_options] =
      CreateDefaultPipeline<GlyphAtlasPipeline>(*context_);
  glyph_atlas_color_pipelines_[default_options] =
      CreateDefaultPipeline<GlyphAtlasColorPipeline>(*context_);
  geometry_color_pipelines_[default_options] =
      CreateDefaultPipeline<GeometryColorPipeline>(*context_);
  yuv_to_rgb_filter_pipelines_[default_options] =
      CreateDefaultPipeline<YUVToRGBFilterPipeline>(*context_);
  porter_duff_blend_pipelines_[default_options] =
      CreateDefaultPipeline<PorterDuffBlendPipeline>(*context_);

  if (context_->GetCapabilities()->SupportsCompute()) {
    auto pipeline_desc =
        PointsComputeShaderPipeline::MakeDefaultPipelineDescriptor(*context_);
    point_field_compute_pipelines_ =
        context_->GetPipelineLibrary()->GetPipeline(pipeline_desc).Get();

    auto uv_pipeline_desc =
        UvComputeShaderPipeline::MakeDefaultPipelineDescriptor(*context_);
    uv_compute_pipelines_ =
        context_->GetPipelineLibrary()->GetPipeline(uv_pipeline_desc).Get();
  }

  auto maybe_pipeline_desc =
      solid_fill_pipelines_[default_options]->GetDescriptor();
  if (maybe_pipeline_desc.has_value()) {
    auto clip_pipeline_descriptor = maybe_pipeline_desc.value();
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
    clip_pipelines_[default_options] =
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
    const std::string& label,
    ISize texture_size,
    const SubpassCallback& subpass_callback,
    bool msaa_enabled) const {
  auto context = GetContext();

  RenderTarget subpass_target;
  if (context->GetCapabilities()->SupportsOffscreenMSAA() && msaa_enabled) {
    subpass_target = RenderTarget::CreateOffscreenMSAA(
        *context, texture_size, SPrintF("%s Offscreen", label.c_str()),
        RenderTarget::kDefaultColorAttachmentConfigMSAA, std::nullopt);
  } else {
    subpass_target = RenderTarget::CreateOffscreen(
        *context, texture_size, SPrintF("%s Offscreen", label.c_str()),
        RenderTarget::kDefaultColorAttachmentConfig, std::nullopt);
  }
  auto subpass_texture = subpass_target.GetRenderTargetTexture();
  if (!subpass_texture) {
    return nullptr;
  }

  auto sub_command_buffer = context->CreateCommandBuffer();
  sub_command_buffer->SetLabel(SPrintF("%s CommandBuffer", label.c_str()));
  if (!sub_command_buffer) {
    return nullptr;
  }

  auto sub_renderpass = sub_command_buffer->CreateRenderPass(subpass_target);
  if (!sub_renderpass) {
    return nullptr;
  }
  sub_renderpass->SetLabel(SPrintF("%s RenderPass", label.c_str()));

  if (!subpass_callback(*this, *sub_renderpass)) {
    return nullptr;
  }

  if (!sub_command_buffer->SubmitCommandsAsync(std::move(sub_renderpass))) {
    return nullptr;
  }

  return subpass_texture;
}

std::shared_ptr<scene::SceneContext> ContentContext::GetSceneContext() const {
  return scene_context_;
}

std::shared_ptr<Tessellator> ContentContext::GetTessellator() const {
  return tessellator_;
}

std::shared_ptr<GlyphAtlasContext> ContentContext::GetGlyphAtlasContext(
    GlyphAtlas::Type type) const {
  return type == GlyphAtlas::Type::kAlphaBitmap ? alpha_glyph_atlas_context_
                                                : color_glyph_atlas_context_;
}

std::shared_ptr<Context> ContentContext::GetContext() const {
  return context_;
}

const Capabilities& ContentContext::GetDeviceCapabilities() const {
  return *context_->GetCapabilities();
}

void ContentContext::SetWireframe(bool wireframe) {
  wireframe_ = wireframe;
}

}  // namespace impeller
