// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/display_list/display_list.h"
#include "flutter/display_list/display_list_builder.h"
#include "flutter/display_list/display_list_test_utils.h"
#include "flutter/flow/raster_cache.h"
#include "flutter/flow/testing/mock_raster_cache.h"
#include "gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace flutter {
namespace testing {

TEST(RasterCache, SimpleInitialization) {
  flutter::RasterCache cache;
  ASSERT_TRUE(true);
}

TEST(RasterCache, ThresholdIsRespectedForSkPicture) {
  size_t threshold = 2;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto picture = GetSamplePicture();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, matrix));
  // 1st access.
  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, matrix));

  // 2nd access.
  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  // Now Prepare should cache it.
  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            picture.get(), true, false, matrix));
  ASSERT_TRUE(cache.Draw(*picture, dummy_canvas));
}

TEST(RasterCache, MetricsOmitUnpopulatedEntries) {
  size_t threshold = 2;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto picture = GetSamplePicture();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, matrix));
  // 1st access.
  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();
  ASSERT_EQ(cache.picture_metrics().total_count(), 0u);
  ASSERT_EQ(cache.picture_metrics().total_bytes(), 0u);
  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, matrix));

  // 2nd access.
  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();
  ASSERT_EQ(cache.picture_metrics().total_count(), 0u);
  ASSERT_EQ(cache.picture_metrics().total_bytes(), 0u);
  cache.PrepareNewFrame();

  // Now Prepare should cache it.
  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            picture.get(), true, false, matrix));
  ASSERT_TRUE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();
  ASSERT_EQ(cache.picture_metrics().total_count(), 1u);
  // 150w * 100h * 4bpp
  ASSERT_EQ(cache.picture_metrics().total_bytes(), 60000u);
}

TEST(RasterCache, ThresholdIsRespectedForDisplayList) {
  size_t threshold = 2;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto display_list = GetSampleDisplayList();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), true, false, matrix));
  // 1st access.
  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), true, false, matrix));

  // 2nd access.
  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  // Now Prepare should cache it.
  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            display_list.get(), true, false, matrix));
  ASSERT_TRUE(cache.Draw(*display_list, dummy_canvas));
}

TEST(RasterCache, AccessThresholdOfZeroDisablesCachingForSkPicture) {
  size_t threshold = 0;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto picture = GetSamplePicture();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, matrix));

  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));
}

TEST(RasterCache, AccessThresholdOfZeroDisablesCachingForDisplayList) {
  size_t threshold = 0;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto display_list = GetSampleDisplayList();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), true, false, matrix));

  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));
}

TEST(RasterCache, PictureCacheLimitPerFrameIsRespectedWhenZeroForSkPicture) {
  size_t picture_cache_limit_per_frame = 0;
  flutter::RasterCache cache(3, picture_cache_limit_per_frame);

  SkMatrix matrix = SkMatrix::I();

  auto picture = GetSamplePicture();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, matrix));

  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));
}

TEST(RasterCache, PictureCacheLimitPerFrameIsRespectedWhenZeroForDisplayList) {
  size_t picture_cache_limit_per_frame = 0;
  flutter::RasterCache cache(3, picture_cache_limit_per_frame);

  SkMatrix matrix = SkMatrix::I();

  auto display_list = GetSampleDisplayList();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), true, false, matrix));

  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));
}

TEST(RasterCache, SweepsRemoveUnusedSkPictures) {
  size_t threshold = 1;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto picture = GetSamplePicture();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, matrix));  // 1
  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            picture.get(), true, false, matrix));  // 2
  ASSERT_TRUE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();

  cache.PrepareNewFrame();
  cache.CleanupAfterFrame();  // Extra frame without a Get image access.

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));
}

TEST(RasterCache, SweepsRemoveUnusedDisplayLists) {
  size_t threshold = 1;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto display_list = GetSampleDisplayList();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), true, false, matrix));  // 1
  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            display_list.get(), true, false, matrix));  // 2
  ASSERT_TRUE(cache.Draw(*display_list, dummy_canvas));

  cache.CleanupAfterFrame();

  cache.PrepareNewFrame();
  cache.CleanupAfterFrame();  // Extra frame without a Get image access.

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));
}

// Construct a cache result whose device target rectangle rounds out to be one
// pixel wider than the cached image.  Verify that it can be drawn without
// triggering any assertions.
TEST(RasterCache, DeviceRectRoundOutForSkPicture) {
  size_t threshold = 1;
  flutter::RasterCache cache(threshold);

  SkPictureRecorder recorder;
  SkRect logical_rect = SkRect::MakeLTRB(28, 0, 354.56731, 310.288);
  recorder.beginRecording(logical_rect);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  recorder.getRecordingCanvas()->drawRect(logical_rect, paint);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

  SkMatrix ctm = SkMatrix::MakeAll(1.3312, 0, 233, 0, 1.3312, 206, 0, 0, 1);

  SkCanvas canvas(100, 100, nullptr);
  canvas.setMatrix(ctm);

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), true, false, ctm));
  ASSERT_FALSE(cache.Draw(*picture, canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            picture.get(), true, false, ctm));
  ASSERT_TRUE(cache.Draw(*picture, canvas));

  canvas.translate(248, 0);
  ASSERT_TRUE(cache.Draw(*picture, canvas));
}

// Construct a cache result whose device target rectangle rounds out to be one
// pixel wider than the cached image.  Verify that it can be drawn without
// triggering any assertions.
TEST(RasterCache, DeviceRectRoundOutForDisplayList) {
  size_t threshold = 1;
  flutter::RasterCache cache(threshold);

  SkRect logical_rect = SkRect::MakeLTRB(28, 0, 354.56731, 310.288);
  DisplayListBuilder builder(logical_rect);
  builder.setColor(SK_ColorRED);
  builder.drawRect(logical_rect);
  sk_sp<DisplayList> display_list = builder.Build();

  SkMatrix ctm = SkMatrix::MakeAll(1.3312, 0, 233, 0, 1.3312, 206, 0, 0, 1);

  SkCanvas canvas(100, 100, nullptr);
  canvas.setMatrix(ctm);

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), true, false, ctm));
  ASSERT_FALSE(cache.Draw(*display_list, canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            display_list.get(), true, false, ctm));
  ASSERT_TRUE(cache.Draw(*display_list, canvas));

  canvas.translate(248, 0);
  ASSERT_TRUE(cache.Draw(*display_list, canvas));
}

TEST(RasterCache, NestedOpCountMetricUsedForSkPicture) {
  size_t threshold = 1;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto picture = GetSampleNestedPicture();
  ASSERT_EQ(picture->approximateOpCount(), 1);
  ASSERT_EQ(picture->approximateOpCount(true), 36);

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             picture.get(), false, false, matrix));
  ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            picture.get(), false, false, matrix));
  ASSERT_TRUE(cache.Draw(*picture, dummy_canvas));
}

TEST(RasterCache, NestedOpCountMetricUsedForDisplayList) {
  size_t threshold = 1;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  auto display_list = GetSampleNestedDisplayList();
  ASSERT_EQ(display_list->op_count(), 1u);
  ASSERT_EQ(display_list->op_count(true), 36u);

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), false, false, matrix));
  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            display_list.get(), false, false, matrix));
  ASSERT_TRUE(cache.Draw(*display_list, dummy_canvas));
}

TEST(RasterCache, NaiveComplexityScoringDisplayList) {
  DisplayListComplexityCalculator* calculator =
      DisplayListNaiveComplexityCalculator::GetInstance();

  size_t threshold = 1;
  flutter::RasterCache cache(threshold);

  SkMatrix matrix = SkMatrix::I();

  // Five raster ops will not be cached
  auto display_list = GetSampleDisplayList(5);
  unsigned int complexity_score = calculator->Compute(display_list.get());

  ASSERT_EQ(complexity_score, 5u);
  ASSERT_EQ(display_list->op_count(), 5u);
  ASSERT_FALSE(calculator->ShouldBeCached(complexity_score));

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), false, false, matrix));
  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), false, false, matrix));
  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));

  // Six raster ops should be cached
  display_list = GetSampleDisplayList(6);
  complexity_score = calculator->Compute(display_list.get());

  ASSERT_EQ(complexity_score, 6u);
  ASSERT_EQ(display_list->op_count(), 6u);
  ASSERT_TRUE(calculator->ShouldBeCached(complexity_score));

  cache.PrepareNewFrame();

  ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                             display_list.get(), false, false, matrix));
  ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));

  cache.CleanupAfterFrame();
  cache.PrepareNewFrame();

  ASSERT_TRUE(cache.Prepare(&preroll_context_holder.preroll_context,
                            display_list.get(), false, false, matrix));
  ASSERT_TRUE(cache.Draw(*display_list, dummy_canvas));
}

TEST(RasterCache, SkPictureWithSingularMatrixIsNotCached) {
  size_t threshold = 2;
  flutter::RasterCache cache(threshold);

  SkMatrix matrices[] = {
      SkMatrix::Scale(0, 1),
      SkMatrix::Scale(1, 0),
      SkMatrix::Skew(1, 1),
  };
  int matrixCount = sizeof(matrices) / sizeof(matrices[0]);

  auto picture = GetSamplePicture();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  for (int i = 0; i < 10; i++) {
    cache.PrepareNewFrame();

    for (int j = 0; j < matrixCount; j++) {
      ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                                 picture.get(), true, false, matrices[j]));
    }

    for (int j = 0; j < matrixCount; j++) {
      dummy_canvas.setMatrix(matrices[j]);
      ASSERT_FALSE(cache.Draw(*picture, dummy_canvas));
    }

    cache.CleanupAfterFrame();
  }
}

TEST(RasterCache, DisplayListWithSingularMatrixIsNotCached) {
  size_t threshold = 2;
  flutter::RasterCache cache(threshold);

  SkMatrix matrices[] = {
      SkMatrix::Scale(0, 1),
      SkMatrix::Scale(1, 0),
      SkMatrix::Skew(1, 1),
  };
  int matrixCount = sizeof(matrices) / sizeof(matrices[0]);

  auto display_list = GetSampleDisplayList();

  SkCanvas dummy_canvas;

  PrerollContextHolder preroll_context_holder = GetSamplePrerollContextHolder();

  for (int i = 0; i < 10; i++) {
    cache.PrepareNewFrame();

    for (int j = 0; j < matrixCount; j++) {
      ASSERT_FALSE(cache.Prepare(&preroll_context_holder.preroll_context,
                                 display_list.get(), true, false, matrices[j]));
    }

    for (int j = 0; j < matrixCount; j++) {
      dummy_canvas.setMatrix(matrices[j]);
      ASSERT_FALSE(cache.Draw(*display_list, dummy_canvas));
    }

    cache.CleanupAfterFrame();
  }
}

TEST(RasterCache, RasterCacheKeyHashFunction) {
  RasterCacheKey::Map<int> map;
  auto hash_function = map.hash_function();
  SkMatrix matrix = SkMatrix::I();
  uint64_t id = 5;
  RasterCacheKey layer_key(id, RasterCacheKeyType::kLayer, matrix);
  RasterCacheKey picture_key(id, RasterCacheKeyType::kPicture, matrix);
  RasterCacheKey display_list_key(id, RasterCacheKeyType::kDisplayList, matrix);
  RasterCacheKey layer_children_key(id, RasterCacheKeyType::kLayerChildren,
                                    matrix);

  auto raster_cache_key_id = RasterCacheKeyID({id});
  auto layer_hash_code = hash_function(layer_key);
  ASSERT_EQ(layer_hash_code, fml::HashCombine(raster_cache_key_id.GetHash(),
                                              RasterCacheKeyType::kLayer));

  auto picture_hash_code = hash_function(picture_key);
  ASSERT_EQ(picture_hash_code, fml::HashCombine(raster_cache_key_id.GetHash(),
                                                RasterCacheKeyType::kPicture));

  auto display_list_hash_code = hash_function(display_list_key);
  ASSERT_EQ(display_list_hash_code,
            fml::HashCombine(raster_cache_key_id.GetHash(),
                             RasterCacheKeyType::kDisplayList));

  auto layer_children_hash_code = hash_function(layer_children_key);
  ASSERT_EQ(layer_children_hash_code,
            fml::HashCombine(raster_cache_key_id.GetHash(),
                             RasterCacheKeyType::kLayerChildren));
}

TEST(RasterCache, RasterCacheKeySameID) {
  RasterCacheKey::Map<int> map;
  SkMatrix matrix = SkMatrix::I();
  uint64_t id = 5;
  RasterCacheKey layer_key(id, RasterCacheKeyType::kLayer, matrix);
  RasterCacheKey picture_key(id, RasterCacheKeyType::kPicture, matrix);
  RasterCacheKey display_list_key(id, RasterCacheKeyType::kDisplayList, matrix);
  RasterCacheKey layer_children_key(id, RasterCacheKeyType::kLayerChildren,
                                    matrix);
  map[layer_key] = 100;
  map[picture_key] = 200;
  map[display_list_key] = 300;
  map[layer_children_key] = 400;

  ASSERT_EQ(map[layer_key], 100);
  ASSERT_EQ(map[picture_key], 200);
  ASSERT_EQ(map[display_list_key], 300);
  ASSERT_EQ(map[layer_children_key], 400);
}

TEST(RasterCache, RasterCacheKeySameType) {
  RasterCacheKey::Map<int> map;
  SkMatrix matrix = SkMatrix::I();

  RasterCacheKeyType type = RasterCacheKeyType::kLayer;
  RasterCacheKey layer_first_key(5, type, matrix);
  RasterCacheKey layer_second_key(10, type, matrix);
  RasterCacheKey layer_third_key(15, type, matrix);
  map[layer_first_key] = 50;
  map[layer_second_key] = 100;
  map[layer_third_key] = 150;
  ASSERT_EQ(map[layer_first_key], 50);
  ASSERT_EQ(map[layer_second_key], 100);
  ASSERT_EQ(map[layer_third_key], 150);

  type = RasterCacheKeyType::kPicture;
  RasterCacheKey picture_first_key(20, type, matrix);
  RasterCacheKey picture_second_key(25, type, matrix);
  RasterCacheKey picture_third_key(30, type, matrix);
  map[picture_first_key] = 200;
  map[picture_second_key] = 250;
  map[picture_third_key] = 300;
  ASSERT_EQ(map[picture_first_key], 200);
  ASSERT_EQ(map[picture_second_key], 250);
  ASSERT_EQ(map[picture_third_key], 300);

  type = RasterCacheKeyType::kDisplayList;
  RasterCacheKey display_list_first_key(35, type, matrix);
  RasterCacheKey display_list_second_key(40, type, matrix);
  RasterCacheKey display_list_third_key(45, type, matrix);
  map[display_list_first_key] = 350;
  map[display_list_second_key] = 400;
  map[display_list_third_key] = 450;
  ASSERT_EQ(map[display_list_first_key], 350);
  ASSERT_EQ(map[display_list_second_key], 400);
  ASSERT_EQ(map[display_list_third_key], 450);

  type = RasterCacheKeyType::kLayerChildren;
  RasterCacheKey layer_children_first_key(RasterCacheKeyID({1, 2, 3}), type,
                                          matrix);
  RasterCacheKey layer_children_second_key(RasterCacheKeyID({2, 3, 1}), type,
                                           matrix);
  RasterCacheKey layer_children_third_key(RasterCacheKeyID({3, 2, 1}), type,
                                          matrix);
  map[layer_children_first_key] = 100;
  map[layer_children_second_key] = 200;
  map[layer_children_third_key] = 300;
  ASSERT_EQ(map[layer_children_first_key], 100);
  ASSERT_EQ(map[layer_children_second_key], 200);
  ASSERT_EQ(map[layer_children_third_key], 300);
}

TEST(RasterCache, RasterCacheKeyID_Equal) {
  RasterCacheKeyID first = RasterCacheKeyID({1});
  RasterCacheKeyID second = RasterCacheKeyID({1});
  RasterCacheKeyID third = RasterCacheKeyID({2});
  ASSERT_EQ(first, second);
  ASSERT_NE(first, third);

  RasterCacheKeyID fourth = RasterCacheKeyID({1, 2});
  RasterCacheKeyID fifth = RasterCacheKeyID({1, 2});
  RasterCacheKeyID sixth = RasterCacheKeyID({2, 1});
  ASSERT_EQ(fourth, fifth);
  ASSERT_NE(fourth, sixth);
}

TEST(RasterCache, RasterCacheKeyID_HashCode) {
  uint64_t foo = 1;
  uint64_t bar = 2;
  RasterCacheKeyID first = RasterCacheKeyID({foo});
  RasterCacheKeyID second = RasterCacheKeyID({foo, bar});
  RasterCacheKeyID third = RasterCacheKeyID({bar, foo});

  ASSERT_EQ(first.GetHash(), fml::HashCombine(foo));
  ASSERT_EQ(second.GetHash(), fml::HashCombine(foo, bar));
  ASSERT_EQ(third.GetHash(), fml::HashCombine(bar, foo));
}

}  // namespace testing
}  // namespace flutter
