// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/color_filter.h"

#include <cstring>

#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"

namespace flutter {

static void ColorFilter_constructor(Dart_NativeArguments args) {
  UIDartState::ThrowIfUIOperationsProhibited();
  DartCallConstructor(&ColorFilter::Create, args);
}

IMPLEMENT_WRAPPERTYPEINFO(ui, ColorFilter);

#define FOR_EACH_BINDING(V)             \
  V(ColorFilter, initMode)              \
  V(ColorFilter, initMatrix)            \
  V(ColorFilter, initSrgbToLinearGamma) \
  V(ColorFilter, initLinearToSrgbGamma)

FOR_EACH_BINDING(DART_NATIVE_CALLBACK)

void ColorFilter::RegisterNatives(tonic::DartLibraryNatives* natives) {
  natives->Register(
      {{"ColorFilter_constructor", ColorFilter_constructor, 1, true},
       FOR_EACH_BINDING(DART_REGISTER_NATIVE)});
}

fml::RefPtr<ColorFilter> ColorFilter::Create() {
  return fml::MakeRefCounted<ColorFilter>();
}

void ColorFilter::initMode(int color, int blend_mode) {
  filter_ = std::make_shared<DlBlendColorFilter>(
      static_cast<DlColor>(color), static_cast<DlBlendMode>(blend_mode));
}

void ColorFilter::initMatrix(const tonic::Float32List& color_matrix) {
  FML_CHECK(color_matrix.num_elements() == 20);

  // Flutter still defines the matrix to be biased by 255 in the last column
  // (translate). skia is normalized, treating the last column as 0...1, so we
  // post-scale here before calling the skia factory.
  float matrix[20];
  memcpy(matrix, color_matrix.data(), sizeof(matrix));
  matrix[4] *= 1.0f / 255;
  matrix[9] *= 1.0f / 255;
  matrix[14] *= 1.0f / 255;
  matrix[19] *= 1.0f / 255;
  filter_ = std::make_shared<DlMatrixColorFilter>(matrix);
}

void ColorFilter::initLinearToSrgbGamma() {
  filter_ = DlLinearToSrgbGammaColorFilter::instance;
}

void ColorFilter::initSrgbToLinearGamma() {
  filter_ = DlSrgbToLinearGammaColorFilter::instance;
}

ColorFilter::~ColorFilter() = default;

}  // namespace flutter
