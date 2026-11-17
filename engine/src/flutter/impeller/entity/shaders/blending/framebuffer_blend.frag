#version 450

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/blending.glsl>
#include <impeller/color.glsl>
#include <impeller/texture.glsl>
#include <impeller/types.glsl>
#include "blend_select.glsl"
#include "coverage_composite.glsl"

// Warning: if any of the constant values or layouts are changed in this
// file, then the hard-coded constant value in
// impeller/renderer/backend/vulkan/binding_helpers_vk.cc
layout(constant_id = 0) const float blend_type = 0;
layout(constant_id = 1) const float supports_decal = 1;

#if defined(IMPELLER_TARGET_VULKAN) && defined(FRAMEBUFFER_BLEND_SINGLE_SAMPLE)
layout(input_attachment_index = 0) uniform subpassInput uSub;
#else
layout(input_attachment_index = 0) uniform subpassInputMS uSub;
#endif

vec4 ReadDestination() {
#if defined(IMPELLER_TARGET_VULKAN) && defined(FRAMEBUFFER_BLEND_SINGLE_SAMPLE)
  return subpassLoad(uSub);
#elif defined(IMPELLER_TARGET_VULKAN)
  // Vulkan reads MSAA input attachments one sample at a time, and this shader
  // runs once per pixel. Average only the samples covered by this primitive.
  // Samples outside its coverage may already have been written earlier in
  // this draw, e.g. along the shared edge of the quad's two triangles.
  // https://github.com/flutter/flutter/issues/179547
  int mask = gl_SampleMaskIn[0] & 0xf;
  vec4 result = vec4(0.0);
  for (int i = 0; i < 4; i++) {
    if ((mask & (1 << i)) != 0) {
      result += subpassLoad(uSub, i);
    }
  }
  return result / float(max(bitCount(mask), 1));
#else
  // On Metal and GLES, SPIRV-Cross turns this into framebuffer fetch and
  // ignores the sample index. Sample variables such as gl_SampleMaskIn also
  // aren't available in GLSL ES 1.00.
  return subpassLoad(uSub, 0);
#endif
}

uniform sampler2D texture_sampler_src;

uniform FragInfo {
  float16_t src_input_alpha;
  float16_t dst_input_alpha;
  vec4 coverage_source_color;
}
frag_info;

in vec2 v_src_texture_coords;

out vec4 frag_color;

vec4 Sample(sampler2D texture_sampler, vec2 texture_coords) {
  if (supports_decal > 0.0) {
    return texture(texture_sampler, texture_coords, kDefaultMipBias);
  }
  return IPSampleDecal(texture_sampler, texture_coords);
}

void main() {
  if (blend_type >= kFirstCoverageBlend) {
    float coverage = Sample(texture_sampler_src, v_src_texture_coords).a;
    if (coverage == 0.0) {
      discard;
    }
    frag_color =
        CompositeCoverageBlend(blend_type, ReadDestination(),
                               frag_info.coverage_source_color, coverage);
    return;
  }
  f16vec4 dst = IPHalfUnpremultiply(f16vec4(ReadDestination()));
  dst.a *= frag_info.dst_input_alpha;
  f16vec4 src = IPHalfUnpremultiply(
      f16vec4(Sample(texture_sampler_src,  // sampler
                     v_src_texture_coords  // texture coordinates
                     )));
  src.a *= frag_info.src_input_alpha;

  f16vec3 blend_result = AdvancedBlend(dst.rgb, src.rgb, int(blend_type));

  frag_color = IPApplyBlendedColor(dst, src, blend_result);
}
