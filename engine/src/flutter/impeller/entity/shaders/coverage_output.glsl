// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Source is premultiplied. The material computes coverage in every variant.
#ifndef COVERAGE_COLOR_TYPE
#define COVERAGE_COLOR_TYPE vec4
#endif

out COVERAGE_COLOR_TYPE frag_color;

// Matches CoverageBlendOutput::kCoverage.
const float kCoverageOutput = 1.0;

void WriteCoverageBlendOutput(COVERAGE_COLOR_TYPE source,
                              float coverage,
                              float output_kind) {
  COVERAGE_COLOR_TYPE mask = COVERAGE_COLOR_TYPE(coverage);
  frag_color = output_kind == kCoverageOutput ? mask : source * mask;
}
