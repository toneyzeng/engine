// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/shader_mask_layer.h"

#include "flutter/flow/layers/opacity_layer.h"
#include "flutter/flow/testing/layer_test.h"
#include "flutter/flow/testing/mock_layer.h"
#include "flutter/fml/macros.h"
#include "flutter/testing/mock_canvas.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkPerlinNoiseShader.h"

namespace flutter {
namespace testing {

using ShaderMaskLayerTest = LayerTest;

#ifndef NDEBUG
TEST_F(ShaderMaskLayerTest, PaintingEmptyLayerDies) {
  auto layer =
      std::make_shared<ShaderMaskLayer>(nullptr, kEmptyRect, SkBlendMode::kSrc);

  layer->Preroll(preroll_context(), SkMatrix());
  EXPECT_EQ(layer->paint_bounds(), kEmptyRect);
  EXPECT_EQ(layer->child_paint_bounds(), kEmptyRect);
  EXPECT_FALSE(layer->needs_painting(paint_context()));

  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),
                            "needs_painting\\(context\\)");
}

TEST_F(ShaderMaskLayerTest, PaintBeforePrerollDies) {
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkPath child_path = SkPath().addRect(child_bounds);
  auto mock_layer = std::make_shared<MockLayer>(child_path);
  auto layer =
      std::make_shared<ShaderMaskLayer>(nullptr, kEmptyRect, SkBlendMode::kSrc);
  layer->Add(mock_layer);

  EXPECT_EQ(layer->paint_bounds(), kEmptyRect);
  EXPECT_EQ(layer->child_paint_bounds(), kEmptyRect);
  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),
                            "needs_painting\\(context\\)");
}
#endif

TEST_F(ShaderMaskLayerTest, EmptyFilter) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkRect layer_bounds = SkRect::MakeLTRB(2.0f, 4.0f, 6.5f, 6.5f);
  const SkPath child_path = SkPath().addRect(child_bounds);
  const SkPaint child_paint = SkPaint(SkColors::kYellow);
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer = std::make_shared<ShaderMaskLayer>(nullptr, layer_bounds,
                                                 SkBlendMode::kSrc);
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(mock_layer->paint_bounds(), child_bounds);
  EXPECT_EQ(layer->paint_bounds(), child_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), child_bounds);
  EXPECT_TRUE(mock_layer->needs_painting(paint_context()));
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer->parent_matrix(), initial_transform);

  SkPaint filter_paint;
  filter_paint.setBlendMode(SkBlendMode::kSrc);
  filter_paint.setShader(nullptr);
  layer->Paint(paint_context());
  EXPECT_EQ(
      mock_canvas().draw_calls(),
      std::vector({MockCanvas::DrawCall{
                       0, MockCanvas::SaveLayerData{child_bounds, SkPaint(),
                                                    nullptr, 1}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawPathData{child_path, child_paint}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::ConcatMatrixData{SkM44::Translate(
                              layer_bounds.fLeft, layer_bounds.fTop)}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawRectData{SkRect::MakeWH(
                                                       layer_bounds.width(),
                                                       layer_bounds.height()),
                                                   filter_paint}},
                   MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));
}

TEST_F(ShaderMaskLayerTest, SimpleFilter) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkRect layer_bounds = SkRect::MakeLTRB(2.0f, 4.0f, 6.5f, 6.5f);
  const SkPath child_path = SkPath().addRect(child_bounds);
  const SkPaint child_paint = SkPaint(SkColors::kYellow);
  auto layer_filter =
      SkPerlinNoiseShader::MakeFractalNoise(1.0f, 1.0f, 1, 1.0f);
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer = std::make_shared<ShaderMaskLayer>(layer_filter, layer_bounds,
                                                 SkBlendMode::kSrc);
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(layer->paint_bounds(), child_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), child_bounds);
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer->parent_matrix(), initial_transform);

  SkPaint filter_paint;
  filter_paint.setBlendMode(SkBlendMode::kSrc);
  filter_paint.setShader(layer_filter);
  layer->Paint(paint_context());
  EXPECT_EQ(
      mock_canvas().draw_calls(),
      std::vector({MockCanvas::DrawCall{
                       0, MockCanvas::SaveLayerData{child_bounds, SkPaint(),
                                                    nullptr, 1}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawPathData{child_path, child_paint}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::ConcatMatrixData{SkM44::Translate(
                              layer_bounds.fLeft, layer_bounds.fTop)}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawRectData{SkRect::MakeWH(
                                                       layer_bounds.width(),
                                                       layer_bounds.height()),
                                                   filter_paint}},
                   MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));
}

TEST_F(ShaderMaskLayerTest, MultipleChildren) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkRect layer_bounds = SkRect::MakeLTRB(2.0f, 4.0f, 6.5f, 6.5f);
  const SkPath child_path1 = SkPath().addRect(child_bounds);
  const SkPath child_path2 =
      SkPath().addRect(child_bounds.makeOffset(3.0f, 0.0f));
  const SkPaint child_paint1 = SkPaint(SkColors::kYellow);
  const SkPaint child_paint2 = SkPaint(SkColors::kCyan);
  auto layer_filter =
      SkPerlinNoiseShader::MakeFractalNoise(1.0f, 1.0f, 1, 1.0f);
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1, child_paint1);
  auto mock_layer2 = std::make_shared<MockLayer>(child_path2, child_paint2);
  auto layer = std::make_shared<ShaderMaskLayer>(layer_filter, layer_bounds,
                                                 SkBlendMode::kSrc);
  layer->Add(mock_layer1);
  layer->Add(mock_layer2);

  SkRect children_bounds = child_path1.getBounds();
  children_bounds.join(child_path2.getBounds());
  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(mock_layer1->paint_bounds(), child_path1.getBounds());
  EXPECT_EQ(mock_layer2->paint_bounds(), child_path2.getBounds());
  EXPECT_EQ(layer->paint_bounds(), children_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), children_bounds);
  EXPECT_TRUE(mock_layer1->needs_painting(paint_context()));
  EXPECT_TRUE(mock_layer2->needs_painting(paint_context()));
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer1->parent_matrix(), initial_transform);
  EXPECT_EQ(mock_layer2->parent_matrix(), initial_transform);

  SkPaint filter_paint;
  filter_paint.setBlendMode(SkBlendMode::kSrc);
  filter_paint.setShader(layer_filter);
  layer->Paint(paint_context());
  EXPECT_EQ(
      mock_canvas().draw_calls(),
      std::vector({MockCanvas::DrawCall{
                       0, MockCanvas::SaveLayerData{children_bounds, SkPaint(),
                                                    nullptr, 1}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawPathData{child_path1, child_paint1}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawPathData{child_path2, child_paint2}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::ConcatMatrixData{SkM44::Translate(
                              layer_bounds.fLeft, layer_bounds.fTop)}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawRectData{SkRect::MakeWH(
                                                       layer_bounds.width(),
                                                       layer_bounds.height()),
                                                   filter_paint}},
                   MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));
}

TEST_F(ShaderMaskLayerTest, Nested) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 7.5f, 8.5f);
  const SkRect layer_bounds = SkRect::MakeLTRB(2.0f, 4.0f, 20.5f, 20.5f);
  const SkPath child_path1 = SkPath().addRect(child_bounds);
  const SkPath child_path2 =
      SkPath().addRect(child_bounds.makeOffset(3.0f, 0.0f));
  const SkPaint child_paint1 = SkPaint(SkColors::kYellow);
  const SkPaint child_paint2 = SkPaint(SkColors::kCyan);
  auto layer_filter1 =
      SkPerlinNoiseShader::MakeFractalNoise(1.0f, 1.0f, 1, 1.0f);
  auto layer_filter2 =
      SkPerlinNoiseShader::MakeFractalNoise(2.0f, 2.0f, 2, 2.0f);
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1, child_paint1);
  auto mock_layer2 = std::make_shared<MockLayer>(child_path2, child_paint2);
  auto layer1 = std::make_shared<ShaderMaskLayer>(layer_filter1, layer_bounds,
                                                  SkBlendMode::kSrc);
  auto layer2 = std::make_shared<ShaderMaskLayer>(layer_filter2, layer_bounds,
                                                  SkBlendMode::kSrc);
  layer2->Add(mock_layer2);
  layer1->Add(mock_layer1);
  layer1->Add(layer2);

  SkRect children_bounds = child_path1.getBounds();
  children_bounds.join(child_path2.getBounds());
  layer1->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(mock_layer1->paint_bounds(), child_path1.getBounds());
  EXPECT_EQ(mock_layer2->paint_bounds(), child_path2.getBounds());
  EXPECT_EQ(layer1->paint_bounds(), children_bounds);
  EXPECT_EQ(layer1->child_paint_bounds(), children_bounds);
  EXPECT_EQ(layer2->paint_bounds(), mock_layer2->paint_bounds());
  EXPECT_EQ(layer2->child_paint_bounds(), mock_layer2->paint_bounds());
  EXPECT_TRUE(mock_layer1->needs_painting(paint_context()));
  EXPECT_TRUE(mock_layer2->needs_painting(paint_context()));
  EXPECT_TRUE(layer1->needs_painting(paint_context()));
  EXPECT_TRUE(layer2->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer1->parent_matrix(), initial_transform);
  EXPECT_EQ(mock_layer2->parent_matrix(), initial_transform);

  SkPaint filter_paint1, filter_paint2;
  filter_paint1.setBlendMode(SkBlendMode::kSrc);
  filter_paint2.setBlendMode(SkBlendMode::kSrc);
  filter_paint1.setShader(layer_filter1);
  filter_paint2.setShader(layer_filter2);
  layer1->Paint(paint_context());
  EXPECT_EQ(
      mock_canvas().draw_calls(),
      std::vector(
          {MockCanvas::DrawCall{
               0, MockCanvas::SaveLayerData{children_bounds, SkPaint(), nullptr,
                                            1}},
           MockCanvas::DrawCall{
               1, MockCanvas::DrawPathData{child_path1, child_paint1}},
           MockCanvas::DrawCall{
               1, MockCanvas::SaveLayerData{child_path2.getBounds(), SkPaint(),
                                            nullptr, 2}},
           MockCanvas::DrawCall{
               2, MockCanvas::DrawPathData{child_path2, child_paint2}},
           MockCanvas::DrawCall{2,
                                MockCanvas::ConcatMatrixData{SkM44::Translate(
                                    layer_bounds.fLeft, layer_bounds.fTop)}},
           MockCanvas::DrawCall{
               2,
               MockCanvas::DrawRectData{
                   SkRect::MakeWH(layer_bounds.width(), layer_bounds.height()),
                   filter_paint2}},
           MockCanvas::DrawCall{2, MockCanvas::RestoreData{1}},
           MockCanvas::DrawCall{1,
                                MockCanvas::ConcatMatrixData{SkM44::Translate(
                                    layer_bounds.fLeft, layer_bounds.fTop)}},
           MockCanvas::DrawCall{
               1,
               MockCanvas::DrawRectData{
                   SkRect::MakeWH(layer_bounds.width(), layer_bounds.height()),
                   filter_paint1}},
           MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));
}

TEST_F(ShaderMaskLayerTest, Readback) {
  auto initial_transform = SkMatrix();
  const SkRect layer_bounds = SkRect::MakeLTRB(2.0f, 4.0f, 20.5f, 20.5f);
  auto layer_filter =
      SkPerlinNoiseShader::MakeFractalNoise(1.0f, 1.0f, 1, 1.0f);
  auto layer = std::make_shared<ShaderMaskLayer>(layer_filter, layer_bounds,
                                                 SkBlendMode::kSrc);

  // ShaderMaskLayer does not read from surface
  preroll_context()->surface_needs_readback = false;
  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_FALSE(preroll_context()->surface_needs_readback);

  // ShaderMaskLayer blocks child with readback
  auto mock_layer =
      std::make_shared<MockLayer>(SkPath(), SkPaint(), false, true);
  layer->Add(mock_layer);
  preroll_context()->surface_needs_readback = false;
  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_FALSE(preroll_context()->surface_needs_readback);
}

TEST_F(ShaderMaskLayerTest, LayerCached) {
  auto layer_filter =
      SkPerlinNoiseShader::MakeFractalNoise(1.0f, 1.0f, 1, 1.0f);
  const SkRect layer_bounds = SkRect::MakeLTRB(2.0f, 4.0f, 20.5f, 20.5f);
  auto initial_transform = SkMatrix::Translate(50.0, 25.5);
  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  auto mock_layer = std::make_shared<MockLayer>(child_path);
  auto layer = std::make_shared<ShaderMaskLayer>(layer_filter, layer_bounds,
                                                 SkBlendMode::kSrc);
  layer->Add(mock_layer);

  SkMatrix cache_ctm = initial_transform;
  SkCanvas cache_canvas;
  cache_canvas.setMatrix(cache_ctm);

  use_mock_raster_cache();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_FALSE(raster_cache()->Draw(layer.get(), cache_canvas,
                                    RasterCacheLayerStrategy::kLayer));

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_FALSE(raster_cache()->Draw(layer.get(), cache_canvas,
                                    RasterCacheLayerStrategy::kLayer));

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_FALSE(raster_cache()->Draw(layer.get(), cache_canvas,
                                    RasterCacheLayerStrategy::kLayer));

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)1);
  EXPECT_TRUE(raster_cache()->Draw(layer.get(), cache_canvas,
                                   RasterCacheLayerStrategy::kLayer));
}

TEST_F(ShaderMaskLayerTest, OpacityInheritance) {
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkPath child_path = SkPath().addRect(child_bounds);
  auto mock_layer = MockLayer::Make(child_path);
  const SkRect mask_rect = SkRect::MakeLTRB(10, 10, 20, 20);
  auto shader_mask_layer =
      std::make_shared<ShaderMaskLayer>(nullptr, mask_rect, SkBlendMode::kSrc);
  shader_mask_layer->Add(mock_layer);

  // ShaderMaskLayers can always support opacity despite incompatible children
  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  shader_mask_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);

  int opacity_alpha = 0x7F;
  SkPoint offset = SkPoint::Make(10, 10);
  auto opacity_layer = std::make_shared<OpacityLayer>(opacity_alpha, offset);
  opacity_layer->Add(shader_mask_layer);
  context->subtree_can_inherit_opacity = false;
  opacity_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(opacity_layer->children_can_accept_opacity());

#ifndef SUPPORT_FRACTIONAL_TRANSLATION
  auto opacity_integer_transform = SkM44::Translate(offset.fX, offset.fY);
#endif
  DisplayListBuilder expected_builder;
  /* OpacityLayer::Paint() */ {
    expected_builder.save();
    {
      expected_builder.translate(offset.fX, offset.fY);
#ifndef SUPPORT_FRACTIONAL_TRANSLATION
      expected_builder.transformReset();
      expected_builder.transform(opacity_integer_transform);
#endif
      /* ShaderMaskLayer::Paint() */ {
        expected_builder.setColor(opacity_alpha << 24);
        expected_builder.saveLayer(&child_path.getBounds(), true);
        {
          /* child layer paint */ {
            expected_builder.setColor(0xFF000000);
            expected_builder.drawPath(child_path);
          }
          expected_builder.translate(mask_rect.fLeft, mask_rect.fTop);
          expected_builder.setBlendMode(DlBlendMode::kSrc);
          expected_builder.drawRect(
              SkRect::MakeWH(mask_rect.width(), mask_rect.height()));
        }
        expected_builder.restore();
      }
    }
    expected_builder.restore();
  }

  opacity_layer->Paint(display_list_paint_context());
  EXPECT_TRUE(DisplayListsEQ_Verbose(expected_builder.Build(), display_list()));
}

}  // namespace testing
}  // namespace flutter
