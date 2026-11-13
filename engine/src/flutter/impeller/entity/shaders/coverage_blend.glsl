// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMPELLER_COVERAGE_BLEND_GLSL_
#define IMPELLER_COVERAGE_BLEND_GLSL_

#ifndef COVERAGE_BLEND_COLOR_TYPE
#define COVERAGE_BLEND_COLOR_TYPE vec4
#endif

#if defined(COVERAGE_BLEND_DUAL_SOURCE)
layout(location = 0, index = 0) out COVERAGE_BLEND_COLOR_TYPE frag_color;
layout(location = 0, index = 1) out COVERAGE_BLEND_COLOR_TYPE frag_coverage;
#else
out COVERAGE_BLEND_COLOR_TYPE frag_color;
#endif

#if defined(COVERAGE_BLEND_FETCH)
// Include before the material's uniforms so Vulkan assigns the input binding.
layout(input_attachment_index = 0) uniform subpassInput blend_destination;

vec4 ReadBlendDestination() {
  return subpassLoad(blend_destination);
}
#elif defined(COVERAGE_BLEND_BACKDROP)
uniform sampler2D backdrop_sampler;
uniform BackdropInfo {
  highp vec4 uv_transform;
}
backdrop_info;

vec4 ReadBlendDestination() {
  return texture(backdrop_sampler,
                 gl_FragCoord.xy * backdrop_info.uv_transform.xy +
                     backdrop_info.uv_transform.zw);
}
#endif

// Matches BlendPlan::Output.
COVERAGE_BLEND_COLOR_TYPE EvaluateBlendOutput(COVERAGE_BLEND_COLOR_TYPE source,
                                              float coverage,
                                              float expression) {
  const float kCoverage = 1.0;
  const float kCompositedColor = 2.0;
  const float kNone = 3.0;
#if defined(COVERAGE_BLEND_FETCH) || defined(COVERAGE_BLEND_BACKDROP)
  if (expression == kCompositedColor) {
    return COVERAGE_BLEND_COLOR_TYPE(
        mix(ReadBlendDestination(), vec4(source), coverage));
  }
#endif
  if (expression == kCoverage) {
    return COVERAGE_BLEND_COLOR_TYPE(coverage);
  }
  if (expression == kNone) {
    return COVERAGE_BLEND_COLOR_TYPE(0.0);
  }
  return source * COVERAGE_BLEND_COLOR_TYPE(coverage);
}

// The material supplies a premultiplied source and its independent coverage.
void WriteCoverageBlend(COVERAGE_BLEND_COLOR_TYPE source,
                        float coverage,
                        vec2 outputs) {
#if defined(COVERAGE_BLEND_FETCH) || defined(COVERAGE_BLEND_BACKDROP)
  if (coverage <= 0.0) {
    discard;
  }
#endif
  frag_color = EvaluateBlendOutput(source, coverage, outputs.x);
#if defined(COVERAGE_BLEND_DUAL_SOURCE)
  frag_coverage = EvaluateBlendOutput(source, coverage, outputs.y);
#endif
}

#endif
