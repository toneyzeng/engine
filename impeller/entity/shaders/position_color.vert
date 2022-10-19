// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/transform.glsl>

uniform VertInfo {
  mat4 mvp;
} vert_info;

in vec2 position;
in vec4 color;

out vec4 v_color;
out vec2 v_position;

void main() {
  gl_Position = vert_info.mvp * vec4(position, 0.0, 1.0);
  v_color = color;
  v_position = IPVec2TransformPosition(vert_info.mvp, position);
}
