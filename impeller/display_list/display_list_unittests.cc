// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "display_list/display_list_blend_mode.h"
#include "display_list/display_list_color.h"
#include "display_list/display_list_color_filter.h"
#include "display_list/display_list_image_filter.h"
#include "display_list/display_list_paint.h"
#include "display_list/display_list_tile_mode.h"
#include "flutter/display_list/display_list_builder.h"
#include "flutter/display_list/display_list_mask_filter.h"
#include "flutter/display_list/types.h"
#include "flutter/testing/testing.h"
#include "impeller/display_list/display_list_image_impeller.h"
#include "impeller/display_list/display_list_playground.h"
#include "impeller/geometry/point.h"
#include "impeller/playground/widgets.h"
#include "include/core/SkRRect.h"
#include "third_party/imgui/imgui.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPathBuilder.h"

namespace impeller {
namespace testing {

using DisplayListTest = DisplayListPlayground;
INSTANTIATE_PLAYGROUND_SUITE(DisplayListTest);

TEST_P(DisplayListTest, CanDrawRect) {
  flutter::DisplayListBuilder builder;
  builder.setColor(SK_ColorBLUE);
  builder.drawRect(SkRect::MakeXYWH(10, 10, 100, 100));
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawTextBlob) {
  flutter::DisplayListBuilder builder;
  builder.setColor(SK_ColorBLUE);
  builder.drawTextBlob(SkTextBlob::MakeFromString("Hello", CreateTestFont()),
                       100, 100);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawImage) {
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;
  builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                    flutter::DlImageSampling::kNearestNeighbor, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawCapsAndJoins) {
  flutter::DisplayListBuilder builder;

  builder.setStyle(flutter::DlDrawStyle::kStroke);
  builder.setStrokeWidth(30);
  builder.setColor(SK_ColorRED);

  auto path =
      SkPathBuilder{}.moveTo(-50, 0).lineTo(0, -50).lineTo(50, 0).snapshot();

  builder.translate(100, 100);
  {
    builder.setStrokeCap(flutter::DlStrokeCap::kButt);
    builder.setStrokeJoin(flutter::DlStrokeJoin::kMiter);
    builder.setStrokeMiter(4);
    builder.drawPath(path);
  }

  {
    builder.save();
    builder.translate(0, 100);
    // The joint in the path is 45 degrees. A miter length of 1 convert to a
    // bevel in this case.
    builder.setStrokeMiter(1);
    builder.drawPath(path);
    builder.restore();
  }

  builder.translate(150, 0);
  {
    builder.setStrokeCap(flutter::DlStrokeCap::kSquare);
    builder.setStrokeJoin(flutter::DlStrokeJoin::kBevel);
    builder.drawPath(path);
  }

  builder.translate(150, 0);
  {
    builder.setStrokeCap(flutter::DlStrokeCap::kRound);
    builder.setStrokeJoin(flutter::DlStrokeJoin::kRound);
    builder.drawPath(path);
  }

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawArc) {
  bool first_frame = true;
  auto callback = [&]() {
    if (first_frame) {
      first_frame = false;
      ImGui::SetNextWindowSize({400, 100});
      ImGui::SetNextWindowPos({300, 550});
    }

    static float start_angle = 45;
    static float sweep_angle = 270;
    static bool use_center = true;

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderFloat("Start angle", &start_angle, -360, 360);
    ImGui::SliderFloat("Sweep angle", &sweep_angle, -360, 360);
    ImGui::Checkbox("Use center", &use_center);
    ImGui::End();

    auto [p1, p2] = IMPELLER_PLAYGROUND_LINE(
        Point(200, 200), Point(400, 400), 20, Color::White(), Color::White());

    flutter::DisplayListBuilder builder;

    Vector2 scale = GetContentScale();
    builder.scale(scale.x, scale.y);
    builder.setStyle(flutter::DlDrawStyle::kStroke);
    builder.setStrokeCap(flutter::DlStrokeCap::kRound);
    builder.setStrokeJoin(flutter::DlStrokeJoin::kMiter);
    builder.setStrokeMiter(10);
    auto rect = SkRect::MakeLTRB(p1.x, p1.y, p2.x, p2.y);
    builder.setColor(SK_ColorGREEN);
    builder.setStrokeWidth(2);
    builder.drawRect(rect);
    builder.setColor(SK_ColorRED);
    builder.setStrokeWidth(10);
    builder.drawArc(rect, start_angle, sweep_angle, use_center);

    return builder.Build();
  };
  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(DisplayListTest, StrokedPathsDrawCorrectly) {
  flutter::DisplayListBuilder builder;
  builder.setColor(SK_ColorRED);
  builder.setStyle(flutter::DlDrawStyle::kStroke);
  builder.setStrokeWidth(10);

  // Rectangle
  builder.translate(100, 100);
  builder.drawRect(SkRect::MakeSize({100, 100}));

  // Rounded rectangle
  builder.translate(150, 0);
  builder.drawRRect(SkRRect::MakeRectXY(SkRect::MakeSize({100, 50}), 10, 10));

  // Double rounded rectangle
  builder.translate(150, 0);
  builder.drawDRRect(
      SkRRect::MakeRectXY(SkRect::MakeSize({100, 50}), 10, 10),
      SkRRect::MakeRectXY(SkRect::MakeXYWH(10, 10, 80, 30), 10, 10));

  // Contour with duplicate join points
  {
    builder.translate(150, 0);
    SkPath path;
    path.lineTo({100, 0});
    path.lineTo({100, 0});
    path.lineTo({100, 100});
    builder.drawPath(path);
  }

  // Contour with duplicate end points
  {
    builder.setStrokeCap(flutter::DlStrokeCap::kRound);
    builder.translate(150, 0);
    SkPath path;
    path.moveTo(0, 0);
    path.lineTo({0, 0});
    path.lineTo({50, 50});
    path.lineTo({100, 0});
    path.lineTo({100, 0});
    builder.drawPath(path);
  }

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawWithOddPathWinding) {
  flutter::DisplayListBuilder builder;
  builder.setColor(SK_ColorRED);
  builder.setStyle(flutter::DlDrawStyle::kFill);

  builder.translate(300, 300);
  SkPath path;
  path.setFillType(SkPathFillType::kEvenOdd);
  path.addCircle(0, 0, 100);
  path.addCircle(0, 0, 50);
  builder.drawPath(path);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawWithMaskBlur) {
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;

  // Mask blurred image.
  {
    auto filter = flutter::DlBlurMaskFilter(kNormal_SkBlurStyle, 10.0f);
    builder.setMaskFilter(&filter);
    builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                      flutter::DlImageSampling::kNearestNeighbor, true);
  }

  // Mask blurred filled path.
  {
    builder.setColor(SK_ColorYELLOW);
    auto filter = flutter::DlBlurMaskFilter(kOuter_SkBlurStyle, 10.0f);
    builder.setMaskFilter(&filter);
    builder.drawArc(SkRect::MakeXYWH(410, 110, 100, 100), 45, 270, true);
  }

  // Mask blurred text.
  {
    auto filter = flutter::DlBlurMaskFilter(kSolid_SkBlurStyle, 10.0f);
    builder.setMaskFilter(&filter);
    builder.drawTextBlob(
        SkTextBlob::MakeFromString("Testing", CreateTestFont()), 220, 170);
  }

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawWithBlendColorFilter) {
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;

  // Pipeline blended image.
  {
    auto filter = flutter::DlBlendColorFilter(SK_ColorYELLOW,
                                              flutter::DlBlendMode::kModulate);
    builder.setColorFilter(&filter);
    builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                      flutter::DlImageSampling::kNearestNeighbor, true);
  }

  // Advanced blended image.
  {
    auto filter =
        flutter::DlBlendColorFilter(SK_ColorRED, flutter::DlBlendMode::kScreen);
    builder.setColorFilter(&filter);
    builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(250, 250),
                      flutter::DlImageSampling::kNearestNeighbor, true);
  }

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawWithColorFilterImageFilter) {
  const float invert_color_matrix[20] = {
      -1, 0,  0,  0, 1,  //
      0,  -1, 0,  0, 1,  //
      0,  0,  -1, 0, 1,  //
      0,  0,  0,  1, 0,  //
  };
  auto texture = CreateTextureForFixture("boston.jpg");
  flutter::DisplayListBuilder builder;
  auto color_filter =
      std::make_shared<flutter::DlMatrixColorFilter>(invert_color_matrix);
  auto image_filter =
      std::make_shared<flutter::DlColorFilterImageFilter>(color_filter);
  builder.setImageFilter(image_filter.get());
  builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                    flutter::DlImageSampling::kNearestNeighbor, true);

  builder.translate(0, 700);
  builder.setColorFilter(color_filter.get());
  builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                    flutter::DlImageSampling::kNearestNeighbor, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawWithImageBlurFilter) {
  auto texture = CreateTextureForFixture("embarcadero.jpg");

  bool first_frame = true;
  auto callback = [&]() {
    if (first_frame) {
      first_frame = false;
      ImGui::SetNextWindowSize({400, 100});
      ImGui::SetNextWindowPos({300, 550});
    }

    static float sigma[] = {10, 10};

    ImGui::Begin("Controls");
    ImGui::SliderFloat2("Sigma", sigma, 0, 100);
    ImGui::End();

    flutter::DisplayListBuilder builder;

    auto filter = flutter::DlBlurImageFilter(sigma[0], sigma[1],
                                             flutter::DlTileMode::kClamp);
    builder.setImageFilter(&filter);
    builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(200, 200),
                      flutter::DlImageSampling::kNearestNeighbor, true);

    return builder.Build();
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(DisplayListTest, CanDrawWithComposeImageFilter) {
  auto texture = CreateTextureForFixture("boston.jpg");
  flutter::DisplayListBuilder builder;
  auto dilate = std::make_shared<flutter::DlDilateImageFilter>(10.0, 10.0);
  auto erode = std::make_shared<flutter::DlErodeImageFilter>(10.0, 10.0);
  auto open = std::make_shared<flutter::DlComposeImageFilter>(dilate, erode);
  auto close = std::make_shared<flutter::DlComposeImageFilter>(erode, dilate);
  builder.setImageFilter(open.get());
  builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                    flutter::DlImageSampling::kNearestNeighbor, true);
  builder.translate(0, 700);
  builder.setImageFilter(close.get());
  builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                    flutter::DlImageSampling::kNearestNeighbor, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanClampTheResultingColorOfColorMatrixFilter) {
  auto texture = CreateTextureForFixture("boston.jpg");
  const float inner_color_matrix[20] = {
      1, 0, 0, 0, 0,  //
      0, 1, 0, 0, 0,  //
      0, 0, 1, 0, 0,  //
      0, 0, 0, 2, 0,  //
  };
  const float outer_color_matrix[20] = {
      1, 0, 0, 0,   0,  //
      0, 1, 0, 0,   0,  //
      0, 0, 1, 0,   0,  //
      0, 0, 0, 0.5, 0,  //
  };
  auto inner_color_filter =
      std::make_shared<flutter::DlMatrixColorFilter>(inner_color_matrix);
  auto outer_color_filter =
      std::make_shared<flutter::DlMatrixColorFilter>(outer_color_matrix);
  auto inner =
      std::make_shared<flutter::DlColorFilterImageFilter>(inner_color_filter);
  auto outer =
      std::make_shared<flutter::DlColorFilterImageFilter>(outer_color_filter);
  auto compose = std::make_shared<flutter::DlComposeImageFilter>(outer, inner);

  flutter::DisplayListBuilder builder;
  builder.setImageFilter(compose.get());
  builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                    flutter::DlImageSampling::kNearestNeighbor, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, SaveLayerWithColorMatrixFiltersAndAlphaDrawCorrectly) {
  auto texture = CreateTextureForFixture("boston.jpg");
  bool first_frame = true;
  enum class Type { kUseAsImageFilter, kUseAsColorFilter, kDisableFilter };
  auto callback = [&]() {
    if (first_frame) {
      first_frame = false;
      ImGui::SetNextWindowPos({10, 10});
    }

    static float alpha = 0.5;
    static int selected_type = 0;
    const char* names[] = {"Use as image filter", "Use as color filter",
                           "Disable filter"};

    static float color_matrix[20] = {
        1, 0, 0, 0, 0,  //
        0, 1, 0, 0, 0,  //
        0, 0, 1, 0, 0,  //
        0, 0, 0, 2, 0,  //
    };

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderFloat("Alpha", &alpha, 0, 1);

    ImGui::Combo("Type", &selected_type, names, sizeof(names) / sizeof(char*));
    std::string label = "##1";
    for (int i = 0; i < 20; i += 5) {
      ImGui::InputScalarN(label.c_str(), ImGuiDataType_Float,
                          &(color_matrix[i]), 5, nullptr, nullptr, "%.2f", 0);
      label[2]++;
    }
    ImGui::End();

    flutter::DisplayListBuilder builder;
    flutter::DlPaint save_paint;
    save_paint.setAlpha(static_cast<uint8_t>(255 * alpha));
    auto color_filter =
        std::make_shared<flutter::DlMatrixColorFilter>(color_matrix);
    Type type = static_cast<Type>(selected_type);
    switch (type) {
      case Type::kUseAsImageFilter: {
        auto image_filter =
            std::make_shared<flutter::DlColorFilterImageFilter>(color_filter);
        save_paint.setImageFilter(image_filter);
        break;
      }
      case Type::kUseAsColorFilter: {
        save_paint.setColorFilter(color_filter);
        break;
      }
      case Type::kDisableFilter:
        break;
    }
    builder.saveLayer(nullptr, &save_paint);
    flutter::DlPaint draw_paint;
    builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(100, 100),
                      flutter::DlImageSampling::kNearestNeighbor, &draw_paint);
    builder.restore();
    return builder.Build();
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(DisplayListTest, SaveLayerWithBlendFiltersAndAlphaDrawCorrectly) {
  auto texture = CreateTextureForFixture("boston.jpg");
  bool first_frame = true;
  enum class Type { kUseAsImageFilter, kUseAsColorFilter, kDisableFilter };
  auto callback = [&]() {
    if (first_frame) {
      first_frame = false;
      ImGui::SetNextWindowPos({10, 10});
    }

    static float alpha = 0.5;
    static int selected_type = 0;
    const char* names[] = {"Use as image filter", "Use as color filter",
                           "Disable filter"};

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderFloat("Alpha", &alpha, 0, 1);

    ImGui::Combo("Type", &selected_type, names, sizeof(names) / sizeof(char*));
    ImGui::End();

    flutter::DisplayListBuilder builder;
    flutter::DlPaint save_paint;
    save_paint.setAlpha(static_cast<uint8_t>(255 * alpha));
    auto color_filter = std::make_shared<flutter::DlBlendColorFilter>(
        flutter::DlColor::kRed(), flutter::DlBlendMode::kDstOver);
    Type type = static_cast<Type>(selected_type);
    switch (type) {
      case Type::kUseAsImageFilter: {
        auto image_filter =
            std::make_shared<flutter::DlColorFilterImageFilter>(color_filter);
        save_paint.setImageFilter(image_filter);
        break;
      }
      case Type::kUseAsColorFilter: {
        save_paint.setColorFilter(color_filter);
        break;
      }
      case Type::kDisableFilter:
        break;
    }
    builder.saveLayer(nullptr, &save_paint);
    flutter::DlPaint draw_paint;
    draw_paint.setColor(flutter::DlColor::kBlue());
    builder.drawRect(SkRect::MakeLTRB(100, 100, 400, 400), draw_paint);
    builder.restore();
    return builder.Build();
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(DisplayListTest, CanDrawBackdropFilter) {
  auto texture = CreateTextureForFixture("embarcadero.jpg");

  bool first_frame = true;
  auto callback = [&]() {
    if (first_frame) {
      first_frame = false;
      ImGui::SetNextWindowPos({10, 10});
    }

    static float sigma[] = {10, 10};
    static float ctm_scale = 1;
    static bool use_bounds = true;
    static bool draw_circle = true;
    static bool add_clip = true;

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderFloat2("Sigma", sigma, 0, 100);
    ImGui::SliderFloat("Scale", &ctm_scale, 0, 10);
    ImGui::NewLine();
    ImGui::TextWrapped(
        "If everything is working correctly, none of the options below should "
        "impact the filter's appearance.");
    ImGui::Checkbox("Use SaveLayer bounds", &use_bounds);
    ImGui::Checkbox("Draw child element", &draw_circle);
    ImGui::Checkbox("Add pre-clip", &add_clip);
    ImGui::End();

    flutter::DisplayListBuilder builder;

    Vector2 scale = ctm_scale * GetContentScale();
    builder.scale(scale.x, scale.y);

    auto filter = flutter::DlBlurImageFilter(sigma[0], sigma[1],
                                             flutter::DlTileMode::kClamp);

    std::optional<SkRect> bounds;
    if (use_bounds) {
      auto [p1, p2] = IMPELLER_PLAYGROUND_LINE(
          Point(350, 150), Point(800, 600), 20, Color::White(), Color::White());
      bounds = SkRect::MakeLTRB(p1.x, p1.y, p2.x, p2.y);
    }

    // Insert a clip to test that the backdrop filter handles stencil depths > 0
    // correctly.
    if (add_clip) {
      builder.clipRect(SkRect::MakeLTRB(0, 0, 99999, 99999),
                       SkClipOp::kIntersect, true);
    }

    builder.drawImage(DlImageImpeller::Make(texture), SkPoint::Make(200, 200),
                      flutter::DlImageSampling::kNearestNeighbor, true);
    builder.saveLayer(bounds.has_value() ? &bounds.value() : nullptr, nullptr,
                      &filter);

    if (draw_circle) {
      auto circle_center =
          IMPELLER_PLAYGROUND_POINT(Point(500, 400), 20, Color::Red());

      builder.setStyle(flutter::DlDrawStyle::kStroke);
      builder.setStrokeCap(flutter::DlStrokeCap::kButt);
      builder.setStrokeJoin(flutter::DlStrokeJoin::kBevel);
      builder.setStrokeWidth(10);
      builder.setColor(flutter::DlColor::kRed().withAlpha(100));
      builder.drawCircle({circle_center.x, circle_center.y}, 100);
    }

    return builder.Build();
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(DisplayListTest, CanDrawNinePatchImage) {
  // Image is drawn with corners to scale and center pieces stretched to fit.
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;
  auto size = texture->GetSize();
  builder.drawImageNine(
      DlImageImpeller::Make(texture),
      SkIRect::MakeLTRB(size.width / 4, size.height / 4, size.width * 3 / 4,
                        size.height * 3 / 4),
      SkRect::MakeLTRB(0, 0, size.width * 2, size.height * 2),
      flutter::DlFilterMode::kNearest, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawNinePatchImageCenterWidthBiggerThanDest) {
  // Edge case, the width of the corners does not leave any room for the
  // center slice. The center (across the vertical axis) is folded out of the
  // resulting image.
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;
  auto size = texture->GetSize();
  builder.drawImageNine(
      DlImageImpeller::Make(texture),
      SkIRect::MakeLTRB(size.width / 4, size.height / 4, size.width * 3 / 4,
                        size.height * 3 / 4),
      SkRect::MakeLTRB(0, 0, size.width / 2, size.height),
      flutter::DlFilterMode::kNearest, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawNinePatchImageCenterHeightBiggerThanDest) {
  // Edge case, the height of the corners does not leave any room for the
  // center slice. The center (across the horizontal axis) is folded out of the
  // resulting image.
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;
  auto size = texture->GetSize();
  builder.drawImageNine(
      DlImageImpeller::Make(texture),
      SkIRect::MakeLTRB(size.width / 4, size.height / 4, size.width * 3 / 4,
                        size.height * 3 / 4),
      SkRect::MakeLTRB(0, 0, size.width, size.height / 2),
      flutter::DlFilterMode::kNearest, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawNinePatchImageCenterBiggerThanDest) {
  // Edge case, the width and height of the corners does not leave any
  // room for the center slices. Only the corners are displayed.
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;
  auto size = texture->GetSize();
  builder.drawImageNine(
      DlImageImpeller::Make(texture),
      SkIRect::MakeLTRB(size.width / 4, size.height / 4, size.width * 3 / 4,
                        size.height * 3 / 4),
      SkRect::MakeLTRB(0, 0, size.width / 2, size.height / 2),
      flutter::DlFilterMode::kNearest, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawNinePatchImageCornersScaledDown) {
  // Edge case, there is not enough room for the corners to be drawn
  // without scaling them down.
  auto texture = CreateTextureForFixture("embarcadero.jpg");
  flutter::DisplayListBuilder builder;
  auto size = texture->GetSize();
  builder.drawImageNine(
      DlImageImpeller::Make(texture),
      SkIRect::MakeLTRB(size.width / 4, size.height / 4, size.width * 3 / 4,
                        size.height * 3 / 4),
      SkRect::MakeLTRB(0, 0, size.width / 4, size.height / 4),
      flutter::DlFilterMode::kNearest, true);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawPoints) {
  flutter::DisplayListBuilder builder;
  SkPoint points[7] = {
      {0, 0},      //
      {100, 100},  //
      {100, 0},    //
      {0, 100},    //
      {0, 0},      //
      {48, 48},    //
      {52, 52},    //
  };
  std::vector<flutter::DlStrokeCap> caps = {
      flutter::DlStrokeCap::kButt,
      flutter::DlStrokeCap::kRound,
      flutter::DlStrokeCap::kSquare,
  };
  flutter::DlPaint paint =
      flutter::DlPaint()                                         //
          .setColor(flutter::DlColor::kYellow().withAlpha(127))  //
          .setStrokeWidth(20);
  builder.translate(50, 50);
  for (auto cap : caps) {
    paint.setStrokeCap(cap);
    builder.save();
    builder.drawPoints(SkCanvas::kPoints_PointMode, 7, points, paint);
    builder.translate(150, 0);
    builder.drawPoints(SkCanvas::kLines_PointMode, 5, points, paint);
    builder.translate(150, 0);
    builder.drawPoints(SkCanvas::kPolygon_PointMode, 5, points, paint);
    builder.restore();
    builder.translate(0, 150);
  }
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawZeroLengthLine) {
  flutter::DisplayListBuilder builder;
  std::vector<flutter::DlStrokeCap> caps = {
      flutter::DlStrokeCap::kButt,
      flutter::DlStrokeCap::kRound,
      flutter::DlStrokeCap::kSquare,
  };
  flutter::DlPaint paint =
      flutter::DlPaint()                                         //
          .setColor(flutter::DlColor::kYellow().withAlpha(127))  //
          .setDrawStyle(flutter::DlDrawStyle::kStroke)           //
          .setStrokeCap(flutter::DlStrokeCap::kButt)             //
          .setStrokeWidth(20);
  SkPath path = SkPath().addPoly({{150, 50}, {150, 50}}, false);
  for (auto cap : caps) {
    paint.setStrokeCap(cap);
    builder.drawLine({50, 50}, {50, 50}, paint);
    builder.drawPath(path, paint);
    builder.translate(0, 150);
  }
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawShadow) {
  flutter::DisplayListBuilder builder;
  std::array<SkPath, 3> paths = {
      SkPath{}.addRect(SkRect::MakeXYWH(0, 0, 200, 100)),
      SkPath{}.addRRect(
          SkRRect::MakeRectXY(SkRect::MakeXYWH(0, 0, 200, 100), 30, 30)),
      SkPath{}.addCircle(100, 50, 50),
  };
  builder.setColor(flutter::DlColor::kWhite());
  builder.drawPaint();
  builder.setColor(flutter::DlColor::kCyan());
  builder.translate(100, 100);
  for (size_t x = 0; x < paths.size(); x++) {
    builder.save();
    for (size_t y = 0; y < 5; y++) {
      builder.drawShadow(paths[x], flutter::DlColor::kBlack(), 3 + y * 5, false,
                         1);
      builder.drawPath(paths[x]);
      builder.translate(0, 200);
    }
    builder.restore();
    builder.translate(300, 0);
  }

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

// Draw a hexagon using triangle fan
TEST_P(DisplayListTest, CanConvertTriangleFanToTriangles) {
  constexpr Scalar hexagon_radius = 125;
  auto hex_start = Point(200.0, -hexagon_radius + 200.0);
  auto center_to_flat = 1.73 / 2 * hexagon_radius;

  // clang-format off
  std::vector<SkPoint> vertices = {
    SkPoint::Make(hex_start.x, hex_start.y),
    SkPoint::Make(hex_start.x + center_to_flat, hex_start.y + 0.5 * hexagon_radius),
    SkPoint::Make(hex_start.x + center_to_flat, hex_start.y + 1.5 * hexagon_radius),
    SkPoint::Make(hex_start.x + center_to_flat, hex_start.y + 1.5 * hexagon_radius),
    SkPoint::Make(hex_start.x, hex_start.y + 2 * hexagon_radius),
    SkPoint::Make(hex_start.x, hex_start.y + 2 * hexagon_radius),
    SkPoint::Make(hex_start.x - center_to_flat, hex_start.y + 1.5 * hexagon_radius),
    SkPoint::Make(hex_start.x - center_to_flat, hex_start.y + 1.5 * hexagon_radius),
    SkPoint::Make(hex_start.x - center_to_flat, hex_start.y + 0.5 * hexagon_radius)
  };
  // clang-format on
  auto paint = flutter::DlPaint().setColor(flutter::DlColor::kDarkGrey());
  auto dl_vertices = flutter::DlVertices::Make(
      flutter::DlVertexMode::kTriangleFan, vertices.size(), vertices.data(),
      nullptr, nullptr);
  flutter::DisplayListBuilder builder;
  builder.drawVertices(dl_vertices, flutter::DlBlendMode::kSrcOver, paint);
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawZeroWidthLine) {
  flutter::DisplayListBuilder builder;
  std::vector<flutter::DlStrokeCap> caps = {
      flutter::DlStrokeCap::kButt,
      flutter::DlStrokeCap::kRound,
      flutter::DlStrokeCap::kSquare,
  };
  flutter::DlPaint paint =                              //
      flutter::DlPaint()                                //
          .setColor(flutter::DlColor::kWhite())         //
          .setDrawStyle(flutter::DlDrawStyle::kStroke)  //
          .setStrokeWidth(0);
  flutter::DlPaint outline_paint =                      //
      flutter::DlPaint()                                //
          .setColor(flutter::DlColor::kYellow())        //
          .setDrawStyle(flutter::DlDrawStyle::kStroke)  //
          .setStrokeCap(flutter::DlStrokeCap::kSquare)  //
          .setStrokeWidth(1);
  SkPath path = SkPath().addPoly({{150, 50}, {160, 50}}, false);
  for (auto cap : caps) {
    paint.setStrokeCap(cap);
    builder.drawLine({50, 50}, {60, 50}, paint);
    builder.drawRect({45, 45, 65, 55}, outline_paint);
    builder.drawLine({100, 50}, {100, 50}, paint);
    if (cap != flutter::DlStrokeCap::kButt) {
      builder.drawRect({95, 45, 105, 55}, outline_paint);
    }
    builder.drawPath(path, paint);
    builder.drawRect(path.getBounds().makeOutset(5, 5), outline_paint);
    builder.translate(0, 150);
  }
  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawWithMatrixFilter) {
  auto boston = CreateTextureForFixture("boston.jpg");

  bool first_frame = true;
  auto callback = [&]() {
    if (first_frame) {
      first_frame = false;
      ImGui::SetNextWindowPos({10, 10});
    }

    static int selected_matrix_type = 0;
    const char* matrix_type_names[] = {"Matrix", "Local Matrix"};

    static float ctm_translation[2] = {200, 200};
    static float ctm_scale[2] = {0.65, 0.65};
    static float ctm_skew[2] = {0, 0};

    static bool enable = true;
    static float translation[2] = {100, 100};
    static float scale[2] = {0.8, 0.8};
    static float skew[2] = {0.2, 0.2};

    static bool enable_savelayer = true;

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    {
      ImGui::Combo("Filter type", &selected_matrix_type, matrix_type_names,
                   sizeof(matrix_type_names) / sizeof(char*));

      ImGui::TextWrapped("Current Transform");
      ImGui::SliderFloat2("CTM Translation", ctm_translation, 0, 1000);
      ImGui::SliderFloat2("CTM Scale", ctm_scale, 0, 3);
      ImGui::SliderFloat2("CTM Skew", ctm_skew, -3, 3);

      ImGui::TextWrapped(
          "MatrixFilter and LocalMatrixFilter modify the CTM in the same way. "
          "The only difference is that MatrixFilter doesn't affect the effect "
          "transform, whereas LocalMatrixFilter does.");
      // Note: See this behavior in:
      //       https://fiddle.skia.org/c/6cbb551ab36d06f163db8693972be954
      ImGui::Checkbox("Enable", &enable);
      ImGui::SliderFloat2("Filter Translation", translation, 0, 1000);
      ImGui::SliderFloat2("Filter Scale", scale, 0, 3);
      ImGui::SliderFloat2("Filter Skew", skew, -3, 3);

      ImGui::TextWrapped(
          "Rendering the filtered image within a layer can expose bounds "
          "issues. If the rendered image gets cut off when this setting is "
          "enabled, there's a coverage bug in the filter.");
      ImGui::Checkbox("Render in layer", &enable_savelayer);
    }
    ImGui::End();

    flutter::DisplayListBuilder builder;
    SkPaint paint;
    if (enable_savelayer) {
      builder.saveLayer(nullptr, nullptr);
    }
    {
      auto content_scale = GetContentScale();
      builder.scale(content_scale.x, content_scale.y);

      // Set the current transform
      auto ctm_matrix =
          SkMatrix::MakeAll(ctm_scale[0], ctm_skew[0], ctm_translation[0],  //
                            ctm_skew[1], ctm_scale[1], ctm_translation[1],  //
                            0, 0, 1);
      builder.transform(ctm_matrix);

      // Set the matrix filter
      auto filter_matrix =
          SkMatrix::MakeAll(scale[0], skew[0], translation[0],  //
                            skew[1], scale[1], translation[1],  //
                            0, 0, 1);

      if (enable) {
        switch (selected_matrix_type) {
          case 0: {
            auto filter = flutter::DlMatrixImageFilter(
                filter_matrix, flutter::DlImageSampling::kLinear);
            builder.setImageFilter(&filter);
            break;
          }
          case 1: {
            auto internal_filter =
                flutter::DlBlurImageFilter(10, 10, flutter::DlTileMode::kDecal)
                    .shared();
            auto filter = flutter::DlLocalMatrixImageFilter(filter_matrix,
                                                            internal_filter);
            builder.setImageFilter(&filter);
            break;
          }
        }
      }

      builder.drawImage(DlImageImpeller::Make(boston), {},
                        flutter::DlImageSampling::kLinear, true);
    }
    if (enable_savelayer) {
      builder.restore();
    }

    return builder.Build();
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(DisplayListTest, CanDrawRectWithLinearToSrgbColorFilter) {
  flutter::DlPaint paint;
  paint.setColor(flutter::DlColor(0xFF2196F3).withAlpha(128));
  flutter::DisplayListBuilder builder;
  paint.setColorFilter(flutter::DlLinearToSrgbGammaColorFilter::instance.get());
  builder.drawRect(SkRect::MakeXYWH(0, 0, 200, 200), paint);
  builder.translate(0, 200);

  paint.setColorFilter(flutter::DlSrgbToLinearGammaColorFilter::instance.get());
  builder.drawRect(SkRect::MakeXYWH(0, 0, 200, 200), paint);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanDrawPaintWithColorSource) {
  const flutter::DlColor colors[2] = {
      flutter::DlColor(0xFFF44336),
      flutter::DlColor(0xFF2196F3),
  };
  const float stops[2] = {0.0, 1.0};
  flutter::DlPaint paint;
  flutter::DisplayListBuilder builder;
  auto clip_bounds = SkRect::MakeWH(300.0, 300.0);
  builder.save();
  builder.translate(100, 100);
  builder.clipRect(clip_bounds, SkClipOp::kIntersect, false);
  auto linear =
      flutter::DlColorSource::MakeLinear({0.0, 0.0}, {100.0, 100.0}, 2, colors,
                                         stops, flutter::DlTileMode::kRepeat);
  paint.setColorSource(linear);
  builder.drawPaint(paint);
  builder.restore();

  builder.save();
  builder.translate(500, 100);
  builder.clipRect(clip_bounds, SkClipOp::kIntersect, false);
  auto radial = flutter::DlColorSource::MakeRadial(
      {100.0, 100.0}, 100.0, 2, colors, stops, flutter::DlTileMode::kRepeat);
  paint.setColorSource(radial);
  builder.drawPaint(paint);
  builder.restore();

  builder.save();
  builder.translate(100, 500);
  builder.clipRect(clip_bounds, SkClipOp::kIntersect, false);
  auto sweep =
      flutter::DlColorSource::MakeSweep({100.0, 100.0}, 180.0, 270.0, 2, colors,
                                        stops, flutter::DlTileMode::kRepeat);
  paint.setColorSource(sweep);
  builder.drawPaint(paint);
  builder.restore();

  builder.save();
  builder.translate(500, 500);
  builder.clipRect(clip_bounds, SkClipOp::kIntersect, false);
  auto texture = CreateTextureForFixture("table_mountain_nx.png");
  auto image = std::make_shared<flutter::DlImageColorSource>(
      DlImageImpeller::Make(texture), flutter::DlTileMode::kRepeat,
      flutter::DlTileMode::kRepeat);
  paint.setColorSource(image);
  builder.drawPaint(paint);
  builder.restore();

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DisplayListTest, CanBlendDstOverAndDstCorrectly) {
  flutter::DisplayListBuilder builder;

  {
    builder.saveLayer(nullptr, nullptr);
    builder.translate(100, 100);
    flutter::DlPaint paint;
    paint.setColor(flutter::DlColor::kRed());
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    paint.setColor(flutter::DlColor::kBlue().withAlpha(127));
    paint.setBlendMode(flutter::DlBlendMode::kSrcOver);
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    builder.restore();
  }
  {
    builder.saveLayer(nullptr, nullptr);
    builder.translate(300, 100);
    flutter::DlPaint paint;
    paint.setColor(flutter::DlColor::kBlue().withAlpha(127));
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    paint.setColor(flutter::DlColor::kRed());
    paint.setBlendMode(flutter::DlBlendMode::kDstOver);
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    builder.restore();
  }
  {
    builder.saveLayer(nullptr, nullptr);
    builder.translate(100, 300);
    flutter::DlPaint paint;
    paint.setColor(flutter::DlColor::kRed());
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    paint.setColor(flutter::DlColor::kBlue().withAlpha(127));
    paint.setBlendMode(flutter::DlBlendMode::kSrc);
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    builder.restore();
  }
  {
    builder.saveLayer(nullptr, nullptr);
    builder.translate(300, 300);
    flutter::DlPaint paint;
    paint.setColor(flutter::DlColor::kBlue().withAlpha(127));
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    paint.setColor(flutter::DlColor::kRed());
    paint.setBlendMode(flutter::DlBlendMode::kDst);
    builder.drawRect(SkRect::MakeSize({200, 200}), paint);
    builder.restore();
  }

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

}  // namespace testing
}  // namespace impeller
