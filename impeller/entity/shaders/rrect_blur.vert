// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/types.glsl>

uniform FrameInfo {
  mat4 mvp;
}
frame_info;

in highp vec2 position;

out highp vec2 v_position;

void main() {
  gl_Position = frame_info.mvp * vec4(position, 0.0, 1.0);
  // The fragment stage uses local coordinates to compute the blur.
  v_position = position;
}
