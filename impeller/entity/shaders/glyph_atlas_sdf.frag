// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

uniform sampler2D glyph_atlas_sampler;

uniform FragInfo {
  vec2 atlas_size;
  vec4 text_color;
} frag_info;

in vec2 v_unit_vertex;
in vec2 v_atlas_position;
in vec2 v_atlas_glyph_size;

out vec4 frag_color;

void main() {
  vec2 scale_perspective = v_atlas_glyph_size / frag_info.atlas_size;
  vec2 offset = v_atlas_position / frag_info.atlas_size;

  // Inspired by Metal by Example's SDF text rendering shader:
  // https://github.com/metal-by-example/sample-code/blob/master/objc/12-TextRendering/TextRendering/Shaders.metal

  // Outline of glyph is the isocontour with value 50%
  float edge_distance = 0.5;
  // Sample the signed-distance field to find distance from this fragment to the glyph outline
  float sample_distance = texture(glyph_atlas_sampler, v_unit_vertex * scale_perspective + offset).a;
  // Use local automatic gradients to find anti-aliased anisotropic edge width, cf. Gustavson 2012
  float edge_width = length(vec2(dFdx(sample_distance), dFdy(sample_distance)));
  // Smooth the glyph edge by interpolating across the boundary in a band with the width determined above
  float insideness = smoothstep(edge_distance - edge_width, edge_distance + edge_width, sample_distance);
  frag_color = frag_info.text_color * insideness;
}
