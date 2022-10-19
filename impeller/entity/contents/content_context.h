// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <unordered_map>

#include "flutter/fml/hash_combine.h"
#include "flutter/fml/macros.h"
#include "fml/logging.h"
#include "impeller/base/validation.h"
#include "impeller/entity/advanced_blend.vert.h"
#include "impeller/entity/advanced_blend_color.frag.h"
#include "impeller/entity/advanced_blend_colorburn.frag.h"
#include "impeller/entity/advanced_blend_colordodge.frag.h"
#include "impeller/entity/advanced_blend_darken.frag.h"
#include "impeller/entity/advanced_blend_difference.frag.h"
#include "impeller/entity/advanced_blend_exclusion.frag.h"
#include "impeller/entity/advanced_blend_hardlight.frag.h"
#include "impeller/entity/advanced_blend_hue.frag.h"
#include "impeller/entity/advanced_blend_lighten.frag.h"
#include "impeller/entity/advanced_blend_luminosity.frag.h"
#include "impeller/entity/advanced_blend_multiply.frag.h"
#include "impeller/entity/advanced_blend_overlay.frag.h"
#include "impeller/entity/advanced_blend_saturation.frag.h"
#include "impeller/entity/advanced_blend_screen.frag.h"
#include "impeller/entity/advanced_blend_softlight.frag.h"
#include "impeller/entity/atlas_fill.frag.h"
#include "impeller/entity/atlas_fill.vert.h"
#include "impeller/entity/blend.frag.h"
#include "impeller/entity/blend.vert.h"
#include "impeller/entity/border_mask_blur.frag.h"
#include "impeller/entity/border_mask_blur.vert.h"
#include "impeller/entity/color_matrix_color_filter.frag.h"
#include "impeller/entity/color_matrix_color_filter.vert.h"
#include "impeller/entity/entity.h"
#include "impeller/entity/gaussian_blur.frag.h"
#include "impeller/entity/gaussian_blur.vert.h"
#include "impeller/entity/glyph_atlas.frag.h"
#include "impeller/entity/glyph_atlas.vert.h"
#include "impeller/entity/glyph_atlas_sdf.frag.h"
#include "impeller/entity/glyph_atlas_sdf.vert.h"
#include "impeller/entity/gradient_fill.vert.h"
#include "impeller/entity/linear_gradient_fill.frag.h"
#include "impeller/entity/linear_to_srgb_filter.frag.h"
#include "impeller/entity/linear_to_srgb_filter.vert.h"
#include "impeller/entity/morphology_filter.frag.h"
#include "impeller/entity/morphology_filter.vert.h"
#include "impeller/entity/radial_gradient_fill.frag.h"
#include "impeller/entity/rrect_blur.frag.h"
#include "impeller/entity/rrect_blur.vert.h"
#include "impeller/entity/solid_fill.frag.h"
#include "impeller/entity/solid_fill.vert.h"
#include "impeller/entity/srgb_to_linear_filter.frag.h"
#include "impeller/entity/srgb_to_linear_filter.vert.h"
#include "impeller/entity/sweep_gradient_fill.frag.h"
#include "impeller/entity/texture_fill.frag.h"
#include "impeller/entity/texture_fill.vert.h"
#include "impeller/entity/tiled_texture_fill.frag.h"
#include "impeller/entity/tiled_texture_fill.vert.h"
#include "impeller/entity/vertices.frag.h"
#include "impeller/renderer/formats.h"
#include "impeller/renderer/pipeline.h"

#include "impeller/entity/position.vert.h"
#include "impeller/entity/position_color.vert.h"
#include "impeller/entity/position_uv.vert.h"

namespace impeller {

using LinearGradientFillPipeline =
    RenderPipelineT<GradientFillVertexShader, LinearGradientFillFragmentShader>;
using SolidFillPipeline =
    RenderPipelineT<SolidFillVertexShader, SolidFillFragmentShader>;
using RadialGradientFillPipeline =
    RenderPipelineT<GradientFillVertexShader, RadialGradientFillFragmentShader>;
using SweepGradientFillPipeline =
    RenderPipelineT<GradientFillVertexShader, SweepGradientFillFragmentShader>;
using BlendPipeline = RenderPipelineT<BlendVertexShader, BlendFragmentShader>;
using RRectBlurPipeline =
    RenderPipelineT<RrectBlurVertexShader, RrectBlurFragmentShader>;
using BlendPipeline = RenderPipelineT<BlendVertexShader, BlendFragmentShader>;
using BlendColorPipeline = RenderPipelineT<AdvancedBlendVertexShader,
                                           AdvancedBlendColorFragmentShader>;
using BlendColorBurnPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendColorburnFragmentShader>;
using BlendColorDodgePipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendColordodgeFragmentShader>;
using BlendDarkenPipeline = RenderPipelineT<AdvancedBlendVertexShader,
                                            AdvancedBlendDarkenFragmentShader>;
using BlendDifferencePipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendDifferenceFragmentShader>;
using BlendExclusionPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendExclusionFragmentShader>;
using BlendHardLightPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendHardlightFragmentShader>;
using BlendHuePipeline =
    RenderPipelineT<AdvancedBlendVertexShader, AdvancedBlendHueFragmentShader>;
using BlendLightenPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendLightenFragmentShader>;
using BlendLuminosityPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendLuminosityFragmentShader>;
using BlendMultiplyPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendMultiplyFragmentShader>;
using BlendOverlayPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendOverlayFragmentShader>;
using BlendSaturationPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendSaturationFragmentShader>;
using BlendScreenPipeline = RenderPipelineT<AdvancedBlendVertexShader,
                                            AdvancedBlendScreenFragmentShader>;
using BlendSoftLightPipeline =
    RenderPipelineT<AdvancedBlendVertexShader,
                    AdvancedBlendSoftlightFragmentShader>;
using TexturePipeline =
    RenderPipelineT<TextureFillVertexShader, TextureFillFragmentShader>;
using TiledTexturePipeline = RenderPipelineT<TiledTextureFillVertexShader,
                                             TiledTextureFillFragmentShader>;
using GaussianBlurPipeline =
    RenderPipelineT<GaussianBlurVertexShader, GaussianBlurFragmentShader>;
using BorderMaskBlurPipeline =
    RenderPipelineT<BorderMaskBlurVertexShader, BorderMaskBlurFragmentShader>;
using MorphologyFilterPipeline =
    RenderPipelineT<MorphologyFilterVertexShader,
                    MorphologyFilterFragmentShader>;
using ColorMatrixColorFilterPipeline =
    RenderPipelineT<ColorMatrixColorFilterVertexShader,
                    ColorMatrixColorFilterFragmentShader>;
using LinearToSrgbFilterPipeline =
    RenderPipelineT<LinearToSrgbFilterVertexShader,
                    LinearToSrgbFilterFragmentShader>;
using SrgbToLinearFilterPipeline =
    RenderPipelineT<SrgbToLinearFilterVertexShader,
                    SrgbToLinearFilterFragmentShader>;
using GlyphAtlasPipeline =
    RenderPipelineT<GlyphAtlasVertexShader, GlyphAtlasFragmentShader>;
using GlyphAtlasSdfPipeline =
    RenderPipelineT<GlyphAtlasSdfVertexShader, GlyphAtlasSdfFragmentShader>;
using AtlasPipeline =
    RenderPipelineT<AtlasFillVertexShader, AtlasFillFragmentShader>;
// Instead of requiring new shaders for clips, the solid fill stages are used
// to redirect writing to the stencil instead of color attachments.
using ClipPipeline =
    RenderPipelineT<SolidFillVertexShader, SolidFillFragmentShader>;

using GeometryPositionPipeline =
    RenderPipelineT<PositionVertexShader, VerticesFragmentShader>;
using GeometryColorPipeline =
    RenderPipelineT<PositionColorVertexShader, VerticesFragmentShader>;

struct ContentContextOptions {
  SampleCount sample_count = SampleCount::kCount1;
  BlendMode blend_mode = BlendMode::kSourceOver;
  CompareFunction stencil_compare = CompareFunction::kEqual;
  StencilOperation stencil_operation = StencilOperation::kKeep;

  struct Hash {
    constexpr std::size_t operator()(const ContentContextOptions& o) const {
      return fml::HashCombine(o.sample_count, o.blend_mode, o.stencil_compare,
                              o.stencil_operation);
    }
  };

  struct Equal {
    constexpr bool operator()(const ContentContextOptions& lhs,
                              const ContentContextOptions& rhs) const {
      return lhs.sample_count == rhs.sample_count &&
             lhs.blend_mode == rhs.blend_mode &&
             lhs.stencil_compare == rhs.stencil_compare &&
             lhs.stencil_operation == rhs.stencil_operation;
    }
  };

  void ApplyToPipelineDescriptor(PipelineDescriptor& desc) const;
};

class Tessellator;

class ContentContext {
 public:
  explicit ContentContext(std::shared_ptr<Context> context);

  ~ContentContext();

  bool IsValid() const;

  std::shared_ptr<Tessellator> GetTessellator() const;

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetLinearGradientFillPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(linear_gradient_fill_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetRadialGradientFillPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(radial_gradient_fill_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetRRectBlurPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(rrect_blur_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetSweepGradientFillPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(sweep_gradient_fill_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetSolidFillPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(solid_fill_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(texture_blend_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetTexturePipeline(
      ContentContextOptions opts) const {
    return GetPipeline(texture_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetTiledTexturePipeline(
      ContentContextOptions opts) const {
    return GetPipeline(tiled_texture_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetGaussianBlurPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(gaussian_blur_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBorderMaskBlurPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(border_mask_blur_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetMorphologyFilterPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(morphology_filter_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>>
  GetColorMatrixColorFilterPipeline(ContentContextOptions opts) const {
    return GetPipeline(color_matrix_color_filter_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetLinearToSrgbFilterPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(linear_to_srgb_filter_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetSrgbToLinearFilterPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(srgb_to_linear_filter_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetClipPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(clip_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetGlyphAtlasPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(glyph_atlas_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetGlyphAtlasSdfPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(glyph_atlas_sdf_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetGeometryColorPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(geometry_color_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetGeometryPositionPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(geometry_position_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetAtlasPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(atlas_pipelines_, opts);
  }

  // Advanced blends.

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendColorPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_color_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendColorBurnPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_colorburn_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendColorDodgePipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_colordodge_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendDarkenPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_darken_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendDifferencePipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_difference_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendExclusionPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_exclusion_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendHardLightPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_hardlight_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendHuePipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_hue_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendLightenPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_lighten_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendLuminosityPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_luminosity_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendMultiplyPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_multiply_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendOverlayPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_overlay_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendSaturationPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_saturation_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendScreenPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_screen_pipelines_, opts);
  }

  std::shared_ptr<Pipeline<PipelineDescriptor>> GetBlendSoftLightPipeline(
      ContentContextOptions opts) const {
    return GetPipeline(blend_softlight_pipelines_, opts);
  }

  std::shared_ptr<Context> GetContext() const;

  using SubpassCallback =
      std::function<bool(const ContentContext&, RenderPass&)>;

  /// @brief  Creates a new texture of size `texture_size` and calls
  ///         `subpass_callback` with a `RenderPass` for drawing to the texture.
  std::shared_ptr<Texture> MakeSubpass(ISize texture_size,
                                       SubpassCallback subpass_callback) const;

 private:
  std::shared_ptr<Context> context_;

  template <class T>
  using Variants = std::unordered_map<ContentContextOptions,
                                      std::unique_ptr<T>,
                                      ContentContextOptions::Hash,
                                      ContentContextOptions::Equal>;

  // These are mutable because while the prototypes are created eagerly, any
  // variants requested from that are lazily created and cached in the variants
  // map.
  mutable Variants<SolidFillPipeline> solid_fill_pipelines_;
  mutable Variants<LinearGradientFillPipeline> linear_gradient_fill_pipelines_;
  mutable Variants<RadialGradientFillPipeline> radial_gradient_fill_pipelines_;
  mutable Variants<SweepGradientFillPipeline> sweep_gradient_fill_pipelines_;
  mutable Variants<RRectBlurPipeline> rrect_blur_pipelines_;
  mutable Variants<BlendPipeline> texture_blend_pipelines_;
  mutable Variants<TexturePipeline> texture_pipelines_;
  mutable Variants<TiledTexturePipeline> tiled_texture_pipelines_;
  mutable Variants<GaussianBlurPipeline> gaussian_blur_pipelines_;
  mutable Variants<BorderMaskBlurPipeline> border_mask_blur_pipelines_;
  mutable Variants<MorphologyFilterPipeline> morphology_filter_pipelines_;
  mutable Variants<ColorMatrixColorFilterPipeline>
      color_matrix_color_filter_pipelines_;
  mutable Variants<LinearToSrgbFilterPipeline> linear_to_srgb_filter_pipelines_;
  mutable Variants<SrgbToLinearFilterPipeline> srgb_to_linear_filter_pipelines_;
  mutable Variants<ClipPipeline> clip_pipelines_;
  mutable Variants<GlyphAtlasPipeline> glyph_atlas_pipelines_;
  mutable Variants<GlyphAtlasSdfPipeline> glyph_atlas_sdf_pipelines_;
  mutable Variants<AtlasPipeline> atlas_pipelines_;
  mutable Variants<GeometryPositionPipeline> geometry_position_pipelines_;
  mutable Variants<GeometryColorPipeline> geometry_color_pipelines_;
  // Advanced blends.
  mutable Variants<BlendColorPipeline> blend_color_pipelines_;
  mutable Variants<BlendColorBurnPipeline> blend_colorburn_pipelines_;
  mutable Variants<BlendColorDodgePipeline> blend_colordodge_pipelines_;
  mutable Variants<BlendDarkenPipeline> blend_darken_pipelines_;
  mutable Variants<BlendDifferencePipeline> blend_difference_pipelines_;
  mutable Variants<BlendExclusionPipeline> blend_exclusion_pipelines_;
  mutable Variants<BlendHardLightPipeline> blend_hardlight_pipelines_;
  mutable Variants<BlendHuePipeline> blend_hue_pipelines_;
  mutable Variants<BlendLightenPipeline> blend_lighten_pipelines_;
  mutable Variants<BlendLuminosityPipeline> blend_luminosity_pipelines_;
  mutable Variants<BlendMultiplyPipeline> blend_multiply_pipelines_;
  mutable Variants<BlendOverlayPipeline> blend_overlay_pipelines_;
  mutable Variants<BlendSaturationPipeline> blend_saturation_pipelines_;
  mutable Variants<BlendScreenPipeline> blend_screen_pipelines_;
  mutable Variants<BlendSoftLightPipeline> blend_softlight_pipelines_;

  template <class TypedPipeline>
  std::shared_ptr<Pipeline<PipelineDescriptor>> GetPipeline(
      Variants<TypedPipeline>& container,
      ContentContextOptions opts) const {
    if (!IsValid()) {
      return nullptr;
    }

    if (auto found = container.find(opts); found != container.end()) {
      return found->second->WaitAndGet();
    }

    auto prototype = container.find({});

    // The prototype must always be initialized in the constructor.
    FML_CHECK(prototype != container.end());

    auto variant_future = prototype->second->WaitAndGet()->CreateVariant(
        [&opts, variants_count = container.size()](PipelineDescriptor& desc) {
          opts.ApplyToPipelineDescriptor(desc);
          desc.SetLabel(
              SPrintF("%s V#%zu", desc.GetLabel().c_str(), variants_count));
        });
    auto variant = std::make_unique<TypedPipeline>(std::move(variant_future));
    auto variant_pipeline = variant->WaitAndGet();
    container[opts] = std::move(variant);
    return variant_pipeline;
  }

  bool is_valid_ = false;
  std::shared_ptr<Tessellator> tessellator_;

  FML_DISALLOW_COPY_AND_ASSIGN(ContentContext);
};

}  // namespace impeller
