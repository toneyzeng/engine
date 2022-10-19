// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/opacity_layer.h"

#include "flutter/flow/layers/clip_rect_layer.h"
#include "flutter/flow/layers/image_filter_layer.h"
#include "flutter/flow/layers/layer_tree.h"
#include "flutter/flow/layers/transform_layer.h"
#include "flutter/flow/raster_cache_util.h"
#include "flutter/flow/testing/diff_context_test.h"
#include "flutter/flow/testing/layer_test.h"
#include "flutter/flow/testing/mock_layer.h"
#include "flutter/fml/macros.h"
#include "flutter/testing/display_list_testing.h"
#include "flutter/testing/mock_canvas.h"
#include "gtest/gtest.h"
#include "include/core/SkPaint.h"

namespace flutter {
namespace testing {

using OpacityLayerTest = LayerTest;

#ifndef NDEBUG
TEST_F(OpacityLayerTest, LeafLayer) {
  auto layer =
      std::make_shared<OpacityLayer>(SK_AlphaOPAQUE, SkPoint::Make(0.0f, 0.0f));

  EXPECT_DEATH_IF_SUPPORTED(layer->Preroll(preroll_context(), SkMatrix()),
                            "\\!layers\\(\\)\\.empty\\(\\)");
}

TEST_F(OpacityLayerTest, PaintingEmptyLayerDies) {
  auto mock_layer = std::make_shared<MockLayer>(SkPath());
  auto layer =
      std::make_shared<OpacityLayer>(SK_AlphaOPAQUE, SkPoint::Make(0.0f, 0.0f));
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), SkMatrix());
  EXPECT_EQ(mock_layer->paint_bounds(), SkPath().getBounds());
  EXPECT_EQ(layer->paint_bounds(), mock_layer->paint_bounds());
  EXPECT_EQ(layer->child_paint_bounds(), mock_layer->paint_bounds());
  EXPECT_FALSE(mock_layer->needs_painting(paint_context()));
  EXPECT_FALSE(layer->needs_painting(paint_context()));

  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),
                            "needs_painting\\(context\\)");
}

TEST_F(OpacityLayerTest, PaintBeforePrerollDies) {
  SkPath child_path;
  child_path.addRect(5.0f, 6.0f, 20.5f, 21.5f);
  auto mock_layer = std::make_shared<MockLayer>(child_path);
  auto layer =
      std::make_shared<OpacityLayer>(SK_AlphaOPAQUE, SkPoint::Make(0.0f, 0.0f));
  layer->Add(mock_layer);

  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),
                            "needs_painting\\(context\\)");
}
#endif

TEST_F(OpacityLayerTest, TranslateChildren) {
  SkPath child_path1;
  child_path1.addRect(10.0f, 10.0f, 20.0f, 20.f);
  SkPaint child_paint1(SkColors::kGray);
  auto layer = std::make_shared<OpacityLayer>(0.5, SkPoint::Make(10, 10));
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1, child_paint1);
  layer->Add(mock_layer1);

  auto initial_transform = SkMatrix::Scale(2.0, 2.0);
  layer->Preroll(preroll_context(), initial_transform);

  SkRect layer_bounds = mock_layer1->paint_bounds();
  mock_layer1->parent_matrix().mapRect(&layer_bounds);

  EXPECT_EQ(layer_bounds, SkRect::MakeXYWH(40, 40, 20, 20));
}

TEST_F(OpacityLayerTest, CacheChild) {
  const SkAlpha alpha_half = 255 / 2;
  auto initial_transform = SkMatrix::Translate(50.0, 25.5);
  auto other_transform = SkMatrix::Scale(1.0, 2.0);
  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  auto mock_layer = std::make_shared<MockLayer>(child_path);
  auto layer =
      std::make_shared<OpacityLayer>(alpha_half, SkPoint::Make(0.0f, 0.0f));
  layer->Add(mock_layer);
  SkPaint paint;

  SkMatrix cache_ctm = initial_transform;
  SkCanvas cache_canvas;
  cache_canvas.setMatrix(cache_ctm);
  SkCanvas other_canvas;
  other_canvas.setMatrix(other_transform);

  use_mock_raster_cache();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);

  const auto* cacheable_opacity_item = layer->raster_cache_item();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_EQ(cacheable_opacity_item->cache_state(),
            RasterCacheItem::CacheState::kNone);
  EXPECT_FALSE(cacheable_opacity_item->GetId().has_value());

  layer->Preroll(preroll_context(), initial_transform);
  LayerTree::TryToRasterCache(cacheable_items(), &paint_context());

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)1);

  EXPECT_EQ(cacheable_opacity_item->cache_state(),
            RasterCacheItem::CacheState::kChildren);
  EXPECT_EQ(
      cacheable_opacity_item->GetId().value(),
      RasterCacheKeyID(RasterCacheKeyID::LayerChildrenIds(layer.get()).value(),
                       RasterCacheKeyType::kLayerChildren));
  EXPECT_FALSE(raster_cache()->Draw(cacheable_opacity_item->GetId().value(),
                                    other_canvas, &paint));
  EXPECT_TRUE(raster_cache()->Draw(cacheable_opacity_item->GetId().value(),
                                   cache_canvas, &paint));
}

TEST_F(OpacityLayerTest, CacheChildren) {
  const SkAlpha alpha_half = 255 / 2;
  auto initial_transform = SkMatrix::Translate(50.0, 25.5);
  auto other_transform = SkMatrix::Scale(1.0, 2.0);
  const SkPath child_path1 = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  const SkPath child_path2 = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  SkPaint paint;
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1);
  auto mock_layer2 = std::make_shared<MockLayer>(child_path2);
  auto layer =
      std::make_shared<OpacityLayer>(alpha_half, SkPoint::Make(0.0f, 0.0f));
  layer->Add(mock_layer1);
  layer->Add(mock_layer2);

  SkMatrix cache_ctm = initial_transform;
  SkCanvas cache_canvas;
  cache_canvas.setMatrix(cache_ctm);
  SkCanvas other_canvas;
  other_canvas.setMatrix(other_transform);

  use_mock_raster_cache();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);

  const auto* cacheable_opacity_item = layer->raster_cache_item();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_EQ(cacheable_opacity_item->cache_state(),
            RasterCacheItem::CacheState::kNone);
  EXPECT_FALSE(cacheable_opacity_item->GetId().has_value());

  layer->Preroll(preroll_context(), initial_transform);
  LayerTree::TryToRasterCache(cacheable_items(), &paint_context());

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)1);

  EXPECT_EQ(cacheable_opacity_item->cache_state(),
            RasterCacheItem::CacheState::kChildren);
  EXPECT_EQ(
      cacheable_opacity_item->GetId().value(),
      RasterCacheKeyID(RasterCacheKeyID::LayerChildrenIds(layer.get()).value(),
                       RasterCacheKeyType::kLayerChildren));
  EXPECT_FALSE(raster_cache()->Draw(cacheable_opacity_item->GetId().value(),
                                    other_canvas, &paint));
  EXPECT_TRUE(raster_cache()->Draw(cacheable_opacity_item->GetId().value(),
                                   cache_canvas, &paint));
}

TEST_F(OpacityLayerTest, ShouldNotCacheChildren) {
  SkPaint paint;
  auto opacity_layer =
      std::make_shared<OpacityLayer>(128, SkPoint::Make(20, 20));
  auto mock_layer = MockLayer::MakeOpacityCompatible(SkPath());
  opacity_layer->Add(mock_layer);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;

  use_mock_raster_cache();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);

  const auto* cacheable_opacity_item = opacity_layer->raster_cache_item();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_EQ(cacheable_opacity_item->cache_state(),
            RasterCacheItem::CacheState::kNone);
  EXPECT_FALSE(cacheable_opacity_item->GetId().has_value());

  opacity_layer->Preroll(preroll_context(), SkMatrix::I());

  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  EXPECT_TRUE(opacity_layer->children_can_accept_opacity());
  LayerTree::TryToRasterCache(cacheable_items(), &paint_context());
  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_EQ(cacheable_opacity_item->cache_state(),
            RasterCacheItem::CacheState::kNone);
  EXPECT_FALSE(cacheable_opacity_item->Draw(paint_context(), &paint));
}

TEST_F(OpacityLayerTest, FullyOpaque) {
  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  const SkPoint layer_offset = SkPoint::Make(0.5f, 1.5f);
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 0.5f);
  const SkMatrix layer_transform =
      SkMatrix::Translate(layer_offset.fX, layer_offset.fY);
  const SkPaint child_paint = SkPaint(SkColors::kGreen);
  const SkRect expected_layer_bounds =
      layer_transform.mapRect(child_path.getBounds());
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer = std::make_shared<OpacityLayer>(SK_AlphaOPAQUE, layer_offset);
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(mock_layer->paint_bounds(), child_path.getBounds());
  EXPECT_EQ(layer->paint_bounds(), expected_layer_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), child_path.getBounds());
  EXPECT_TRUE(mock_layer->needs_painting(paint_context()));
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer->parent_matrix(),
            SkMatrix::Concat(initial_transform, layer_transform));
  EXPECT_EQ(mock_layer->parent_mutators(),
            std::vector({Mutator(layer_transform), Mutator(SK_AlphaOPAQUE)}));

  const SkPaint opacity_paint = SkPaint(SkColors::kBlack);  // A = 1.0f
  SkRect opacity_bounds;
  expected_layer_bounds.makeOffset(-layer_offset.fX, -layer_offset.fY)
      .roundOut(&opacity_bounds);
  auto expected_draw_calls =
      std::vector({MockCanvas::DrawCall{0, MockCanvas::SaveData{1}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::ConcatMatrixData{SkM44(layer_transform)}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::SaveLayerData{opacity_bounds,
                                                    opacity_paint, nullptr, 2}},
                   MockCanvas::DrawCall{
                       2, MockCanvas::DrawPathData{child_path, child_paint}},
                   MockCanvas::DrawCall{2, MockCanvas::RestoreData{1}},
                   MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}});
  layer->Paint(paint_context());
  EXPECT_EQ(mock_canvas().draw_calls(), expected_draw_calls);
}

TEST_F(OpacityLayerTest, FullyTransparent) {
  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  const SkPoint layer_offset = SkPoint::Make(0.5f, 1.5f);
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 0.5f);
  const SkMatrix layer_transform =
      SkMatrix::Translate(layer_offset.fX, layer_offset.fY);
  const SkPaint child_paint = SkPaint(SkColors::kGreen);
  const SkRect expected_layer_bounds =
      layer_transform.mapRect(child_path.getBounds());
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer =
      std::make_shared<OpacityLayer>(SK_AlphaTRANSPARENT, layer_offset);
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(mock_layer->paint_bounds(), child_path.getBounds());
  EXPECT_EQ(layer->paint_bounds(), expected_layer_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), child_path.getBounds());
  EXPECT_TRUE(mock_layer->needs_painting(paint_context()));
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer->parent_matrix(),
            SkMatrix::Concat(initial_transform, layer_transform));
  EXPECT_EQ(
      mock_layer->parent_mutators(),
      std::vector({Mutator(layer_transform), Mutator(SK_AlphaTRANSPARENT)}));

  auto expected_draw_calls = std::vector(
      {MockCanvas::DrawCall{0, MockCanvas::SaveData{1}},
       MockCanvas::DrawCall{
           1, MockCanvas::ConcatMatrixData{SkM44(layer_transform)}},
       MockCanvas::DrawCall{1, MockCanvas::SaveData{2}},
       MockCanvas::DrawCall{
           2, MockCanvas::ClipRectData{kEmptyRect, SkClipOp::kIntersect,
                                       MockCanvas::kHard_ClipEdgeStyle}},
       MockCanvas::DrawCall{2, MockCanvas::RestoreData{1}},
       MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}});
  layer->Paint(paint_context());
  EXPECT_EQ(mock_canvas().draw_calls(), expected_draw_calls);
}

TEST_F(OpacityLayerTest, HalfTransparent) {
  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  const SkPoint layer_offset = SkPoint::Make(0.5f, 1.5f);
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 0.5f);
  const SkMatrix layer_transform =
      SkMatrix::Translate(layer_offset.fX, layer_offset.fY);
  const SkPaint child_paint = SkPaint(SkColors::kGreen);
  const SkRect expected_layer_bounds =
      layer_transform.mapRect(child_path.getBounds());
  const SkAlpha alpha_half = 255 / 2;
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer = std::make_shared<OpacityLayer>(alpha_half, layer_offset);
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(mock_layer->paint_bounds(), child_path.getBounds());
  EXPECT_EQ(layer->paint_bounds(), expected_layer_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), child_path.getBounds());
  EXPECT_TRUE(mock_layer->needs_painting(paint_context()));
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer->parent_matrix(),
            SkMatrix::Concat(initial_transform, layer_transform));
  EXPECT_EQ(mock_layer->parent_mutators(),
            std::vector({Mutator(layer_transform), Mutator(alpha_half)}));

  SkRect opacity_bounds;
  expected_layer_bounds.makeOffset(-layer_offset.fX, -layer_offset.fY)
      .roundOut(&opacity_bounds);
  DlPaint save_paint = DlPaint().setAlpha(alpha_half);
  DlPaint child_dl_paint = DlPaint().setColor(DlColor::kGreen());

  auto expected_builder = DisplayListBuilder();
  expected_builder.save();
  expected_builder.translate(layer_offset.fX, layer_offset.fY);
  expected_builder.saveLayer(&opacity_bounds, &save_paint);
  expected_builder.drawPath(child_path, child_dl_paint);
  expected_builder.restore();
  expected_builder.restore();
  sk_sp<DisplayList> expected_display_list = expected_builder.Build();

  layer->Paint(display_list_paint_context());
  EXPECT_TRUE(DisplayListsEQ_Verbose(display_list(), expected_display_list));
}

TEST_F(OpacityLayerTest, Nested) {
  const SkPath child1_path = SkPath().addRect(SkRect::MakeWH(5.0f, 6.0f));
  const SkPath child2_path = SkPath().addRect(SkRect::MakeWH(2.0f, 7.0f));
  const SkPath child3_path = SkPath().addRect(SkRect::MakeWH(6.0f, 6.0f));
  const SkPoint layer1_offset = SkPoint::Make(0.5f, 1.5f);
  const SkPoint layer2_offset = SkPoint::Make(2.5f, 0.5f);
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 0.5f);
  const SkMatrix layer1_transform =
      SkMatrix::Translate(layer1_offset.fX, layer1_offset.fY);
  const SkMatrix layer2_transform =
      SkMatrix::Translate(layer2_offset.fX, layer2_offset.fY);
  const SkPaint child1_paint = SkPaint(SkColors::kRed);
  const SkPaint child2_paint = SkPaint(SkColors::kBlue);
  const SkPaint child3_paint = SkPaint(SkColors::kGreen);
  const SkAlpha alpha1 = 155;
  const SkAlpha alpha2 = 224;
  auto mock_layer1 = std::make_shared<MockLayer>(child1_path, child1_paint);
  auto mock_layer2 = std::make_shared<MockLayer>(child2_path, child2_paint);
  auto mock_layer3 = std::make_shared<MockLayer>(child3_path, child3_paint);
  auto layer1 = std::make_shared<OpacityLayer>(alpha1, layer1_offset);
  auto layer2 = std::make_shared<OpacityLayer>(alpha2, layer2_offset);
  layer2->Add(mock_layer2);
  layer1->Add(mock_layer1);
  layer1->Add(layer2);
  layer1->Add(mock_layer3);  // Ensure something is processed after recursion

  const SkRect expected_layer2_bounds =
      layer2_transform.mapRect(child2_path.getBounds());
  SkRect layer1_child_bounds = expected_layer2_bounds;
  layer1_child_bounds.join(child1_path.getBounds());
  layer1_child_bounds.join(child3_path.getBounds());
  SkRect expected_layer1_bounds = layer1_transform.mapRect(layer1_child_bounds);
  layer1->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(mock_layer1->paint_bounds(), child1_path.getBounds());
  EXPECT_EQ(mock_layer2->paint_bounds(), child2_path.getBounds());
  EXPECT_EQ(mock_layer3->paint_bounds(), child3_path.getBounds());
  EXPECT_EQ(layer1->paint_bounds(), expected_layer1_bounds);
  EXPECT_EQ(layer1->child_paint_bounds(), layer1_child_bounds);
  EXPECT_EQ(layer2->paint_bounds(), expected_layer2_bounds);
  EXPECT_EQ(layer2->child_paint_bounds(), child2_path.getBounds());
  EXPECT_TRUE(mock_layer1->needs_painting(paint_context()));
  EXPECT_TRUE(mock_layer2->needs_painting(paint_context()));
  EXPECT_TRUE(mock_layer3->needs_painting(paint_context()));
  EXPECT_TRUE(layer1->needs_painting(paint_context()));
  EXPECT_TRUE(layer2->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer1->parent_matrix(),
            SkMatrix::Concat(initial_transform, layer1_transform));
  //   EXPECT_EQ(mock_layer1->parent_mutators(),
  //             std::vector({Mutator(layer1_transform), Mutator(alpha1)}));
  EXPECT_EQ(
      mock_layer2->parent_matrix(),
      SkMatrix::Concat(SkMatrix::Concat(initial_transform, layer1_transform),
                       layer2_transform));
  //   EXPECT_EQ(mock_layer2->parent_mutators(),
  //             std::vector({Mutator(layer1_transform), Mutator(alpha1),
  //                          Mutator(layer2_transform), Mutator(alpha2)}));
  EXPECT_EQ(mock_layer3->parent_matrix(),
            SkMatrix::Concat(initial_transform, layer1_transform));
  //   EXPECT_EQ(mock_layer3->parent_mutators(),
  //             std::vector({Mutator(layer1_transform), Mutator(alpha1)}));

  SkPaint opacity1_paint;
  opacity1_paint.setAlphaf(alpha1 * (1.0 / SK_AlphaOPAQUE));
  SkPaint opacity2_paint;
  opacity2_paint.setAlphaf(alpha2 * (1.0 / SK_AlphaOPAQUE));
  SkRect opacity1_bounds, opacity2_bounds;
  expected_layer1_bounds.makeOffset(-layer1_offset.fX, -layer1_offset.fY)
      .roundOut(&opacity1_bounds);
  expected_layer2_bounds.makeOffset(-layer2_offset.fX, -layer2_offset.fY)
      .roundOut(&opacity2_bounds);
  auto expected_draw_calls = std::vector(
      {MockCanvas::DrawCall{0, MockCanvas::SaveData{1}},
       MockCanvas::DrawCall{
           1, MockCanvas::ConcatMatrixData{SkM44(layer1_transform)}},
       MockCanvas::DrawCall{
           1, MockCanvas::SaveLayerData{opacity1_bounds, opacity1_paint,
                                        nullptr, 2}},
       MockCanvas::DrawCall{
           2, MockCanvas::DrawPathData{child1_path, child1_paint}},
       MockCanvas::DrawCall{2, MockCanvas::SaveData{3}},
       MockCanvas::DrawCall{
           3, MockCanvas::ConcatMatrixData{SkM44(layer2_transform)}},
       MockCanvas::DrawCall{
           3, MockCanvas::SaveLayerData{opacity2_bounds, opacity2_paint,
                                        nullptr, 4}},
       MockCanvas::DrawCall{
           4, MockCanvas::DrawPathData{child2_path, child2_paint}},
       MockCanvas::DrawCall{4, MockCanvas::RestoreData{3}},
       MockCanvas::DrawCall{3, MockCanvas::RestoreData{2}},
       MockCanvas::DrawCall{
           2, MockCanvas::DrawPathData{child3_path, child3_paint}},
       MockCanvas::DrawCall{2, MockCanvas::RestoreData{1}},
       MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}});
  layer1->Paint(paint_context());
  EXPECT_EQ(mock_canvas().draw_calls(), expected_draw_calls);
}

TEST_F(OpacityLayerTest, Readback) {
  auto initial_transform = SkMatrix();
  auto layer = std::make_shared<OpacityLayer>(kOpaque_SkAlphaType, SkPoint());
  layer->Add(std::make_shared<MockLayer>(SkPath()));

  // OpacityLayer does not read from surface
  preroll_context()->surface_needs_readback = false;
  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_FALSE(preroll_context()->surface_needs_readback);

  // OpacityLayer blocks child with readback
  auto mock_layer = std::make_shared<MockLayer>(SkPath(), SkPaint());
  mock_layer->set_fake_reads_surface(true);
  layer->Add(mock_layer);
  preroll_context()->surface_needs_readback = false;
  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_FALSE(preroll_context()->surface_needs_readback);
}

TEST_F(OpacityLayerTest, CullRectIsTransformed) {
  auto clip_rect_layer = std::make_shared<ClipRectLayer>(
      SkRect::MakeLTRB(0, 0, 10, 10), flutter::hardEdge);
  auto opacity_layer =
      std::make_shared<OpacityLayer>(128, SkPoint::Make(20, 20));
  auto mock_layer = std::make_shared<MockLayer>(SkPath());
  clip_rect_layer->Add(opacity_layer);
  opacity_layer->Add(mock_layer);
  clip_rect_layer->Preroll(preroll_context(), SkMatrix::I());
  EXPECT_EQ(mock_layer->parent_cull_rect().fLeft, -20);
  EXPECT_EQ(mock_layer->parent_cull_rect().fTop, -20);
}

TEST_F(OpacityLayerTest, OpacityInheritanceCompatibleChild) {
  auto opacity_layer =
      std::make_shared<OpacityLayer>(128, SkPoint::Make(20, 20));
  auto mock_layer = MockLayer::MakeOpacityCompatible(SkPath());
  opacity_layer->Add(mock_layer);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  opacity_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  EXPECT_TRUE(opacity_layer->children_can_accept_opacity());
}

TEST_F(OpacityLayerTest, OpacityInheritanceIncompatibleChild) {
  auto opacity_layer =
      std::make_shared<OpacityLayer>(128, SkPoint::Make(20, 20));
  auto mock_layer = MockLayer::Make(SkPath());
  opacity_layer->Add(mock_layer);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  opacity_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  EXPECT_FALSE(opacity_layer->children_can_accept_opacity());
}

TEST_F(OpacityLayerTest, OpacityInheritanceThroughContainer) {
  auto opacity_layer =
      std::make_shared<OpacityLayer>(128, SkPoint::Make(20, 20));
  auto container_layer = std::make_shared<ContainerLayer>();
  auto mock_layer = MockLayer::MakeOpacityCompatible(SkPath());
  container_layer->Add(mock_layer);
  opacity_layer->Add(container_layer);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  opacity_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  // By default a container layer will not pass opacity through to
  // its children - specific subclasses will have to enable this
  // pass through by setting the flag to true themselves before
  // calling their super method ContainerLayer::Preroll().
  EXPECT_FALSE(opacity_layer->children_can_accept_opacity());
}

TEST_F(OpacityLayerTest, OpacityInheritanceThroughTransform) {
  auto opacity_layer =
      std::make_shared<OpacityLayer>(128, SkPoint::Make(20, 20));
  auto transformLayer = std::make_shared<TransformLayer>(SkMatrix::Scale(2, 2));
  auto mock_layer = MockLayer::MakeOpacityCompatible(SkPath());
  transformLayer->Add(mock_layer);
  opacity_layer->Add(transformLayer);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  opacity_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  EXPECT_TRUE(opacity_layer->children_can_accept_opacity());
}

TEST_F(OpacityLayerTest, OpacityInheritanceThroughImageFilter) {
  auto opacity_layer =
      std::make_shared<OpacityLayer>(128, SkPoint::Make(20, 20));
  auto filter_layer = std::make_shared<ImageFilterLayer>(
      std::make_shared<DlBlurImageFilter>(5.0, 5.0, DlTileMode::kClamp));
  auto mock_layer = MockLayer::MakeOpacityCompatible(SkPath());
  filter_layer->Add(mock_layer);
  opacity_layer->Add(filter_layer);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  opacity_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  EXPECT_TRUE(opacity_layer->children_can_accept_opacity());
}

TEST_F(OpacityLayerTest, OpacityInheritanceNestedWithCompatibleChild) {
  SkPoint offset1 = SkPoint::Make(10, 20);
  SkPoint offset2 = SkPoint::Make(20, 10);
  SkPath mock_path = SkPath::Rect({10, 10, 20, 20});
  auto opacity_layer_1 = std::make_shared<OpacityLayer>(128, offset1);
  auto opacity_layer_2 = std::make_shared<OpacityLayer>(64, offset2);
  auto mock_layer = MockLayer::MakeOpacityCompatible(mock_path);
  opacity_layer_2->Add(mock_layer);
  opacity_layer_1->Add(opacity_layer_2);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  opacity_layer_1->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  EXPECT_TRUE(opacity_layer_1->children_can_accept_opacity());
  EXPECT_TRUE(opacity_layer_2->children_can_accept_opacity());

  SkPaint savelayer_paint;
  SkScalar inherited_opacity = 128 * 1.0 / SK_AlphaOPAQUE;
  inherited_opacity *= 64 * 1.0 / SK_AlphaOPAQUE;
  savelayer_paint.setAlphaf(inherited_opacity);

  DisplayListBuilder expected_builder;
  /* opacity_layer_1::Paint */ {
    expected_builder.save();
    {
      expected_builder.translate(offset1.fX, offset1.fY);
      /* opacity_layer_2::Paint */ {
        expected_builder.save();
        {
          expected_builder.translate(offset2.fX, offset2.fY);
          /* mock_layer::Paint */ {
            expected_builder.setColor(savelayer_paint.getAlpha() << 24);
            expected_builder.saveLayer(&mock_path.getBounds(), true);
            {
              expected_builder.setColor(0xFF000000);
              expected_builder.drawPath(mock_path);
            }
            expected_builder.restore();
          }
        }
        expected_builder.restore();
      }
    }
    expected_builder.restore();
  }

  opacity_layer_1->Paint(display_list_paint_context());
  EXPECT_TRUE(DisplayListsEQ_Verbose(expected_builder.Build(), display_list()));
}

TEST_F(OpacityLayerTest, OpacityInheritanceNestedWithIncompatibleChild) {
  SkPoint offset1 = SkPoint::Make(10, 20);
  SkPoint offset2 = SkPoint::Make(20, 10);
  SkPath mock_path = SkPath::Rect({10, 10, 20, 20});
  auto opacity_layer_1 = std::make_shared<OpacityLayer>(128, offset1);
  auto opacity_layer_2 = std::make_shared<OpacityLayer>(64, offset2);
  auto mock_layer = MockLayer::Make(mock_path);
  opacity_layer_2->Add(mock_layer);
  opacity_layer_1->Add(opacity_layer_2);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  opacity_layer_1->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(context->subtree_can_inherit_opacity);
  EXPECT_TRUE(opacity_layer_1->children_can_accept_opacity());
  EXPECT_FALSE(opacity_layer_2->children_can_accept_opacity());

  SkPaint savelayer_paint;
  SkScalar inherited_opacity = 128 * 1.0 / SK_AlphaOPAQUE;
  inherited_opacity *= 64 * 1.0 / SK_AlphaOPAQUE;
  savelayer_paint.setAlphaf(inherited_opacity);

  DisplayListBuilder expected_builder;
  /* opacity_layer_1::Paint */ {
    expected_builder.save();
    {
      expected_builder.translate(offset1.fX, offset1.fY);
      /* opacity_layer_2::Paint */ {
        expected_builder.save();
        {
          expected_builder.translate(offset2.fX, offset2.fY);
          expected_builder.setColor(savelayer_paint.getAlpha() << 24);
          expected_builder.saveLayer(&mock_layer->paint_bounds(), true);
          /* mock_layer::Paint */ {
            expected_builder.setColor(0xFF000000);
            expected_builder.drawPath(mock_path);
          }
        }
        expected_builder.restore();
      }
    }
    expected_builder.restore();
  }

  opacity_layer_1->Paint(display_list_paint_context());
  EXPECT_TRUE(DisplayListsEQ_Verbose(expected_builder.Build(), display_list()));
}

using OpacityLayerDiffTest = DiffContextTest;

TEST_F(OpacityLayerDiffTest, FractionalTranslation) {
  auto picture = CreateDisplayListLayer(
      CreateDisplayList(SkRect::MakeLTRB(10, 10, 60, 60), 1));
  auto layer = CreateOpacityLater({picture}, 128, SkPoint::Make(0.5, 0.5));

  MockLayerTree tree1;
  tree1.root()->Add(layer);

  auto damage = DiffLayerTree(tree1, MockLayerTree(), SkIRect::MakeEmpty(), 0,
                              0, /*use_raster_cache=*/false);
  EXPECT_EQ(damage.frame_damage, SkIRect::MakeLTRB(10, 10, 61, 61));
}

TEST_F(OpacityLayerDiffTest, FractionalTranslationWithRasterCache) {
  auto picture = CreateDisplayListLayer(
      CreateDisplayList(SkRect::MakeLTRB(10, 10, 60, 60), 1));
  auto layer = CreateOpacityLater({picture}, 128, SkPoint::Make(0.5, 0.5));

  MockLayerTree tree1;
  tree1.root()->Add(layer);

  auto damage = DiffLayerTree(tree1, MockLayerTree(), SkIRect::MakeEmpty(), 0,
                              0, /*use_raster_cache=*/true);
  EXPECT_EQ(damage.frame_damage, SkIRect::MakeLTRB(11, 11, 61, 61));
}

TEST_F(OpacityLayerTest, FullyOpaqueWithFractionalValues) {
  use_mock_raster_cache();  // Ensure non-fractional alignment.

  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  const SkPoint layer_offset = SkPoint::Make(0.5f, 1.5f);
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 0.5f);
  const SkMatrix layer_transform =
      SkMatrix::Translate(layer_offset.fX, layer_offset.fY);
  const SkPaint child_paint = SkPaint(SkColors::kGreen);
  const SkRect expected_layer_bounds =
      layer_transform.mapRect(child_path.getBounds());
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer = std::make_shared<OpacityLayer>(SK_AlphaOPAQUE, layer_offset);
  layer->Add(mock_layer);
  layer->Preroll(preroll_context(), initial_transform);

  const SkPaint opacity_paint = SkPaint(SkColors::kBlack);  // A = 1.0f
  SkRect opacity_bounds;
  expected_layer_bounds.makeOffset(-layer_offset.fX, -layer_offset.fY)
      .roundOut(&opacity_bounds);
  auto expected_draw_calls = std::vector(
      {MockCanvas::DrawCall{0, MockCanvas::SaveData{1}},
       MockCanvas::DrawCall{
           1, MockCanvas::ConcatMatrixData{SkM44(layer_transform)}},
       MockCanvas::DrawCall{
           1, MockCanvas::SetMatrixData{SkM44(
                  RasterCacheUtil::GetIntegralTransCTM(layer_transform))}},
       MockCanvas::DrawCall{
           1, MockCanvas::SaveLayerData{opacity_bounds, opacity_paint, nullptr,
                                        2}},
       MockCanvas::DrawCall{2,
                            MockCanvas::DrawPathData{child_path, child_paint}},
       MockCanvas::DrawCall{2, MockCanvas::RestoreData{1}},
       MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}});
  layer->Paint(paint_context());
  EXPECT_EQ(mock_canvas().draw_calls(), expected_draw_calls);
}

}  // namespace testing
}  // namespace flutter
