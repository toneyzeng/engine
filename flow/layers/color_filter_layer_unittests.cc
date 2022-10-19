// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/display_list/display_list.h"
#include "flutter/display_list/display_list_color.h"
#include "flutter/display_list/display_list_paint.h"
#include "flutter/flow/compositor_context.h"
#include "flutter/flow/layers/color_filter_layer.h"

#include "flutter/display_list/display_list_color_filter.h"
#include "flutter/flow/layers/layer_tree.h"
#include "flutter/flow/layers/opacity_layer.h"
#include "flutter/flow/raster_cache.h"
#include "flutter/flow/raster_cache_key.h"
#include "flutter/flow/testing/layer_test.h"
#include "flutter/flow/testing/mock_layer.h"
#include "flutter/fml/macros.h"
#include "flutter/testing/mock_canvas.h"
#include "include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"

namespace flutter {
namespace testing {

using ColorFilterLayerTest = LayerTest;

#ifndef NDEBUG
TEST_F(ColorFilterLayerTest, PaintingEmptyLayerDies) {
  auto layer = std::make_shared<ColorFilterLayer>(
      std::make_shared<DlUnknownColorFilter>(sk_sp<SkColorFilter>()));

  layer->Preroll(preroll_context(), SkMatrix());
  EXPECT_EQ(layer->paint_bounds(), kEmptyRect);
  EXPECT_EQ(layer->child_paint_bounds(), kEmptyRect);
  EXPECT_FALSE(layer->needs_painting(paint_context()));

  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),
                            "needs_painting\\(context\\)");
}

TEST_F(ColorFilterLayerTest, PaintBeforePrerollDies) {
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkPath child_path = SkPath().addRect(child_bounds);
  auto mock_layer = std::make_shared<MockLayer>(child_path);

  auto layer = std::make_shared<ColorFilterLayer>(
      std::make_shared<DlUnknownColorFilter>(sk_sp<SkColorFilter>()));
  layer->Add(mock_layer);

  EXPECT_EQ(layer->paint_bounds(), kEmptyRect);
  EXPECT_EQ(layer->child_paint_bounds(), kEmptyRect);
  EXPECT_DEATH_IF_SUPPORTED(layer->Paint(paint_context()),
                            "needs_painting\\(context\\)");
}
#endif

TEST_F(ColorFilterLayerTest, EmptyFilter) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkPath child_path = SkPath().addRect(child_bounds);
  const SkPaint child_paint = SkPaint(SkColors::kYellow);
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer = std::make_shared<ColorFilterLayer>(nullptr);
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(layer->paint_bounds(), child_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), child_bounds);
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer->parent_matrix(), initial_transform);

  SkPaint filter_paint;
  filter_paint.setColorFilter(nullptr);
  layer->Paint(paint_context());
  EXPECT_EQ(
      mock_canvas().draw_calls(),
      std::vector({MockCanvas::DrawCall{
                       0, MockCanvas::SaveLayerData{child_bounds, filter_paint,
                                                    nullptr, 1}},
                   MockCanvas::DrawCall{
                       1, MockCanvas::DrawPathData{child_path, child_paint}},
                   MockCanvas::DrawCall{1, MockCanvas::RestoreData{0}}}));
}

TEST_F(ColorFilterLayerTest, SimpleFilter) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 20.5f, 21.5f);
  const SkPath child_path = SkPath().addRect(child_bounds);
  const SkPaint child_paint = SkPaint(SkColors::kYellow);

  auto dl_color_filter = DlLinearToSrgbGammaColorFilter::instance;
  auto mock_layer = std::make_shared<MockLayer>(child_path, child_paint);
  auto layer = std::make_shared<ColorFilterLayer>(dl_color_filter);
  layer->Add(mock_layer);

  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_EQ(layer->paint_bounds(), child_bounds);
  EXPECT_EQ(layer->child_paint_bounds(), child_bounds);
  EXPECT_TRUE(layer->needs_painting(paint_context()));
  EXPECT_EQ(mock_layer->parent_matrix(), initial_transform);

  DisplayListBuilder expected_builder;
  /* ColorFilterLayer::Paint() */ {
    DlPaint dl_paint;
    dl_paint.setColorFilter(dl_color_filter.get());
    expected_builder.saveLayer(&child_bounds, &dl_paint);
    {
      /* MockLayer::Paint() */ {
        expected_builder.drawPath(child_path,
                                  DlPaint().setColor(DlColor::kYellow()));
      }
    }
  }
  expected_builder.restore();
  auto expected_display_list = expected_builder.Build();

  layer->Paint(display_list_paint_context());
  EXPECT_TRUE(DisplayListsEQ_Verbose(display_list(), expected_display_list));
}

TEST_F(ColorFilterLayerTest, MultipleChildren) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 2.5f, 3.5f);
  const SkPath child_path1 = SkPath().addRect(child_bounds);
  const SkPath child_path2 =
      SkPath().addRect(child_bounds.makeOffset(3.0f, 0.0f));
  const SkPaint child_paint1 = SkPaint(SkColors::kYellow);
  const SkPaint child_paint2 = SkPaint(SkColors::kCyan);
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1, child_paint1);
  auto mock_layer2 = std::make_shared<MockLayer>(child_path2, child_paint2);
  auto dl_color_filter = DlSrgbToLinearGammaColorFilter::instance;
  auto layer = std::make_shared<ColorFilterLayer>(dl_color_filter);
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

  DisplayListBuilder expected_builder;
  /* ColorFilterLayer::Paint() */ {
    DlPaint dl_paint;
    dl_paint.setColorFilter(dl_color_filter.get());
    expected_builder.saveLayer(&children_bounds, &dl_paint);
    {
      /* MockLayer::Paint() */ {
        expected_builder.drawPath(child_path1,
                                  DlPaint().setColor(DlColor::kYellow()));
      }
      /* MockLayer::Paint() */ {
        expected_builder.drawPath(child_path2,
                                  DlPaint().setColor(DlColor::kCyan()));
      }
    }
  }
  expected_builder.restore();
  auto expected_display_list = expected_builder.Build();

  layer->Paint(display_list_paint_context());
  EXPECT_TRUE(DisplayListsEQ_Verbose(display_list(), expected_display_list));
}

TEST_F(ColorFilterLayerTest, Nested) {
  const SkMatrix initial_transform = SkMatrix::Translate(0.5f, 1.0f);
  const SkRect child_bounds = SkRect::MakeLTRB(5.0f, 6.0f, 2.5f, 3.5f);
  const SkPath child_path1 = SkPath().addRect(child_bounds);
  const SkPath child_path2 =
      SkPath().addRect(child_bounds.makeOffset(3.0f, 0.0f));
  const SkPaint child_paint1 = SkPaint(SkColors::kYellow);
  const SkPaint child_paint2 = SkPaint(SkColors::kCyan);
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1, child_paint1);
  auto mock_layer2 = std::make_shared<MockLayer>(child_path2, child_paint2);
  auto dl_color_filter = DlSrgbToLinearGammaColorFilter::instance;
  auto layer1 = std::make_shared<ColorFilterLayer>(dl_color_filter);

  auto layer2 = std::make_shared<ColorFilterLayer>(dl_color_filter);
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

  DisplayListBuilder expected_builder;
  /* ColorFilterLayer::Paint() */ {
    DlPaint dl_paint;
    dl_paint.setColorFilter(dl_color_filter.get());
    expected_builder.saveLayer(&children_bounds, &dl_paint);
    {
      /* MockLayer::Paint() */ {
        expected_builder.drawPath(child_path1,
                                  DlPaint().setColor(DlColor::kYellow()));
      }
      /* ColorFilter::Paint() */ {
        DlPaint child_dl_paint;
        child_dl_paint.setColor(DlColor::kBlack());
        child_dl_paint.setColorFilter(dl_color_filter.get());
        expected_builder.saveLayer(&child_path2.getBounds(), &child_dl_paint);

        /* MockLayer::Paint() */ {
          expected_builder.drawPath(child_path2,
                                    DlPaint().setColor(DlColor::kCyan()));
        }
        expected_builder.restore();
      }
    }
  }
  expected_builder.restore();

  auto expected_display_list = expected_builder.Build();

  layer1->Paint(display_list_paint_context());
  EXPECT_TRUE(DisplayListsEQ_Verbose(display_list(), expected_display_list));
}

TEST_F(ColorFilterLayerTest, Readback) {
  auto initial_transform = SkMatrix();

  // ColorFilterLayer does not read from surface
  auto layer = std::make_shared<ColorFilterLayer>(
      DlLinearToSrgbGammaColorFilter::instance);
  preroll_context()->surface_needs_readback = false;
  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_FALSE(preroll_context()->surface_needs_readback);

  // ColorFilterLayer blocks child with readback
  auto mock_layer = std::make_shared<MockLayer>(SkPath(), SkPaint());
  mock_layer->set_fake_reads_surface(true);
  layer->Add(mock_layer);
  preroll_context()->surface_needs_readback = false;
  layer->Preroll(preroll_context(), initial_transform);
  EXPECT_FALSE(preroll_context()->surface_needs_readback);
}

TEST_F(ColorFilterLayerTest, CacheChild) {
  auto layer_filter = DlSrgbToLinearGammaColorFilter::instance;
  auto initial_transform = SkMatrix::Translate(50.0, 25.5);
  auto other_transform = SkMatrix::Scale(1.0, 2.0);
  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  SkPaint paint = SkPaint();
  auto mock_layer = std::make_shared<MockLayer>(child_path);
  auto layer = std::make_shared<ColorFilterLayer>(layer_filter);
  layer->Add(mock_layer);

  SkMatrix cache_ctm = initial_transform;
  SkCanvas cache_canvas;
  cache_canvas.setMatrix(cache_ctm);
  SkCanvas other_canvas;
  other_canvas.setMatrix(other_transform);

  use_mock_raster_cache();
  const auto* cacheable_color_filter_item = layer->raster_cache_item();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  EXPECT_EQ(cacheable_color_filter_item->cache_state(),
            RasterCacheItem::CacheState::kNone);
  EXPECT_FALSE(cacheable_color_filter_item->GetId().has_value());

  layer->Preroll(preroll_context(), initial_transform);
  LayerTree::TryToRasterCache(cacheable_items(), &paint_context());

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)1);
  EXPECT_EQ(cacheable_color_filter_item->cache_state(),
            RasterCacheItem::CacheState::kChildren);
  EXPECT_EQ(
      cacheable_color_filter_item->GetId().value(),
      RasterCacheKeyID(RasterCacheKeyID::LayerChildrenIds(layer.get()).value(),
                       RasterCacheKeyType::kLayerChildren));
  EXPECT_FALSE(raster_cache()->Draw(
      cacheable_color_filter_item->GetId().value(), other_canvas, &paint));
  EXPECT_TRUE(raster_cache()->Draw(cacheable_color_filter_item->GetId().value(),
                                   cache_canvas, &paint));
}

TEST_F(ColorFilterLayerTest, CacheChildren) {
  auto layer_filter = DlSrgbToLinearGammaColorFilter::instance;
  auto initial_transform = SkMatrix::Translate(50.0, 25.5);
  auto other_transform = SkMatrix::Scale(1.0, 2.0);
  const SkPath child_path1 = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  const SkPath child_path2 = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1);
  auto mock_layer2 = std::make_shared<MockLayer>(child_path2);
  auto layer = std::make_shared<ColorFilterLayer>(layer_filter);
  layer->Add(mock_layer1);
  layer->Add(mock_layer2);
  SkPaint paint = SkPaint();

  SkMatrix cache_ctm = initial_transform;
  SkCanvas cache_canvas;
  cache_canvas.setMatrix(cache_ctm);
  SkCanvas other_canvas;
  other_canvas.setMatrix(other_transform);

  use_mock_raster_cache();

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);
  const auto* cacheable_color_filter_item = layer->raster_cache_item();
  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)0);

  EXPECT_EQ(cacheable_color_filter_item->cache_state(),
            RasterCacheItem::CacheState::kNone);
  EXPECT_FALSE(cacheable_color_filter_item->GetId().has_value());

  layer->Preroll(preroll_context(), initial_transform);
  LayerTree::TryToRasterCache(cacheable_items(), &paint_context());

  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)1);
  EXPECT_EQ(cacheable_color_filter_item->cache_state(),
            RasterCacheItem::CacheState::kChildren);
  EXPECT_EQ(
      cacheable_color_filter_item->GetId().value(),
      RasterCacheKeyID(RasterCacheKeyID::LayerChildrenIds(layer.get()).value(),
                       RasterCacheKeyType::kLayerChildren));
  EXPECT_FALSE(raster_cache()->Draw(
      cacheable_color_filter_item->GetId().value(), other_canvas, &paint));
  EXPECT_TRUE(raster_cache()->Draw(cacheable_color_filter_item->GetId().value(),
                                   cache_canvas, &paint));
}

TEST_F(ColorFilterLayerTest, CacheColorFilterLayerSelf) {
  auto layer_filter = DlSrgbToLinearGammaColorFilter::instance;
  auto initial_transform = SkMatrix::Translate(50.0, 25.5);
  auto other_transform = SkMatrix::Scale(1.0, 2.0);
  const SkPath child_path1 = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  const SkPath child_path2 = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  auto mock_layer1 = std::make_shared<MockLayer>(child_path1);
  auto mock_layer2 = std::make_shared<MockLayer>(child_path2);
  auto layer = std::make_shared<ColorFilterLayer>(layer_filter);
  layer->Add(mock_layer1);
  layer->Add(mock_layer2);
  SkPaint paint = SkPaint();

  SkMatrix cache_ctm = initial_transform;
  SkCanvas cache_canvas;
  cache_canvas.setMatrix(cache_ctm);
  SkCanvas other_canvas;
  other_canvas.setMatrix(other_transform);

  use_mock_raster_cache();
  const auto* cacheable_color_filter_item = layer->raster_cache_item();

  // frame 1.
  layer->Preroll(preroll_context(), initial_transform);
  layer->Paint(paint_context());
  // frame 2.
  layer->Preroll(preroll_context(), initial_transform);
  LayerTree::TryToRasterCache(cacheable_items(), &paint_context());
  // ColorFilterLayer default cache children.
  EXPECT_EQ(cacheable_color_filter_item->cache_state(),
            RasterCacheItem::CacheState::kChildren);
  EXPECT_TRUE(raster_cache()->Draw(cacheable_color_filter_item->GetId().value(),
                                   cache_canvas, &paint));
  EXPECT_FALSE(raster_cache()->Draw(
      cacheable_color_filter_item->GetId().value(), other_canvas, &paint));
  layer->Paint(paint_context());

  // frame 3.
  layer->Preroll(preroll_context(), initial_transform);
  layer->Paint(paint_context());

  LayerTree::TryToRasterCache(cacheable_items(), &paint_context());
  // frame1,2 cache the ColorFilterLayer's children layer, frame3 cache the
  // ColorFilterLayer
  EXPECT_EQ(raster_cache()->GetLayerCachedEntriesCount(), (size_t)2);

  // ColorFilterLayer default cache itself.
  EXPECT_EQ(cacheable_color_filter_item->cache_state(),
            RasterCacheItem::CacheState::kCurrent);
  EXPECT_EQ(cacheable_color_filter_item->GetId(),
            RasterCacheKeyID(layer->unique_id(), RasterCacheKeyType::kLayer));
  EXPECT_TRUE(raster_cache()->Draw(cacheable_color_filter_item->GetId().value(),
                                   cache_canvas, &paint));
  EXPECT_FALSE(raster_cache()->Draw(
      cacheable_color_filter_item->GetId().value(), other_canvas, &paint));
}

TEST_F(ColorFilterLayerTest, OpacityInheritance) {
  // clang-format off
  float matrix[20] = {
    0, 1, 0, 0, 0,
    0, 0, 1, 0, 0,
    1, 0, 0, 0, 0,
    0, 0, 0, 1, 0,
  };
  // clang-format on
  auto layer_filter = DlMatrixColorFilter(matrix);
  auto initial_transform = SkMatrix::Translate(50.0, 25.5);
  const SkPath child_path = SkPath().addRect(SkRect::MakeWH(5.0f, 5.0f));
  auto mock_layer = std::make_shared<MockLayer>(child_path);
  auto color_filter_layer = std::make_shared<ColorFilterLayer>(
      std::make_shared<DlMatrixColorFilter>(matrix));
  color_filter_layer->Add(mock_layer);

  PrerollContext* context = preroll_context();
  context->subtree_can_inherit_opacity = false;
  color_filter_layer->Preroll(preroll_context(), initial_transform);
  // ColorFilterLayer can always inherit opacity whether or not their
  // children are compatible.
  EXPECT_TRUE(context->subtree_can_inherit_opacity);

  int opacity_alpha = 0x7F;
  SkPoint offset = SkPoint::Make(10, 10);
  auto opacity_layer = std::make_shared<OpacityLayer>(opacity_alpha, offset);
  opacity_layer->Add(color_filter_layer);
  context->subtree_can_inherit_opacity = false;
  opacity_layer->Preroll(context, SkMatrix::I());
  EXPECT_TRUE(opacity_layer->children_can_accept_opacity());

  DisplayListBuilder expected_builder;
  /* OpacityLayer::Paint() */ {
    expected_builder.save();
    {
      expected_builder.translate(offset.fX, offset.fY);
      /* ColorFilterLayer::Paint() */ {
        DlPaint dl_paint;
        dl_paint.setColor(opacity_alpha << 24);
        dl_paint.setColorFilter(&layer_filter);
        expected_builder.saveLayer(&child_path.getBounds(), &dl_paint);
        /* MockLayer::Paint() */ {
          expected_builder.drawPath(child_path, DlPaint().setColor(0xFF000000));
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
