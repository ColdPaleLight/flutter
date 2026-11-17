// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

precision mediump float;

#include <impeller/types.glsl>

uniform f16sampler2D glyph_atlas_sampler;

layout(constant_id = 0) const float use_alpha_color_channel = 1.0;

uniform FragInfo {
  float is_color_glyph;
  float use_text_color;
  f16vec4 text_color;
  float text_contrast;
  float coverage_blend_output;
}
frag_info;

in highp vec2 v_uv;

#define COVERAGE_COLOR_TYPE f16vec4
#include "coverage_output.glsl"

void main() {
  f16vec4 value = texture(glyph_atlas_sampler, v_uv);

  if (frag_info.is_color_glyph == 1.0) {
    if (frag_info.use_text_color == 1.0) {
      frag_color = value.aaaa * frag_info.text_color;
    } else {
      frag_color = value * frag_info.text_color.aaaa;
    }
  } else {
    float coverage;
    if (use_alpha_color_channel == 1.0) {
      coverage = float(value.a);
    } else {
      coverage = float(value.r);
    }

    if (frag_info.text_contrast != 1.0) {
      // Applies gamma correction to take into account we are performing in
      // linear space but some fonts are expecting to operate in gamma space.
      // https://www.desmos.com/calculator/gsblcyg5vv
      coverage = 1.0 - pow(1.0 - coverage, frag_info.text_contrast);
    }

    WriteCoverageBlendOutput(frag_info.text_color, coverage,
                             frag_info.coverage_blend_output);
  }
}
