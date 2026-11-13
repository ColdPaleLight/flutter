// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

#include <impeller/types.glsl>

#define COVERAGE_BLEND_COLOR_TYPE f16vec4
#include "coverage_blend.glsl"

uniform f16sampler2D glyph_atlas_sampler;

layout(constant_id = 0) const float use_alpha_color_channel = 1.0;

uniform FragInfo {
  float is_color_glyph;
  float use_text_color;
  f16vec4 text_color;
  float text_contrast;
  vec2 blend_outputs;
}
frag_info;

in highp vec2 v_uv;

void main() {
  f16vec4 value = texture(glyph_atlas_sampler, v_uv);

#if !defined(COVERAGE_BLEND_DUAL_SOURCE) && !defined(COVERAGE_BLEND_FETCH) && \
    !defined(COVERAGE_BLEND_BACKDROP)
  if (frag_info.is_color_glyph == 1.0) {
    if (frag_info.use_text_color == 1.0) {
      frag_color = value.aaaa * frag_info.text_color;
    } else {
      frag_color = value * frag_info.text_color.aaaa;
    }
    return;
  }
#endif

  float coverage = float(use_alpha_color_channel == 1.0 ? value.a : value.r);
  if (frag_info.text_contrast != 1.0) {
    // Applies gamma correction to take into account we are performing in
    // linear space but some fonts are expecting to operate in gamma space.
    // https://www.desmos.com/calculator/1+pow(1-0.5,2.2)
    coverage = 1.0 - pow(1.0 - coverage, frag_info.text_contrast);
  }

  WriteCoverageBlend(frag_info.text_color, coverage, frag_info.blend_outputs);
}
