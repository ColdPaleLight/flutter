// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CoverageBlendProgram values follow the advanced blend specializations.
const float kFirstCoverageBlend = 15.0;
const float kSrcCoverageBlend = kFirstCoverageBlend;

vec4 CompositeCoverageBlend(float blend_type,
                            vec4 dst,
                            vec4 src,
                            float coverage) {
  vec4 blended = dst;
  if (blend_type == kSrcCoverageBlend) {
    blended = src;
  }
  return mix(dst, blended, coverage);
}
