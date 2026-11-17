// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_

#include <optional>
#include <variant>

#include "impeller/core/formats.h"
#include "impeller/geometry/color.h"

namespace impeller {

class Capabilities;
class RenderTarget;

// These values are uniforms consumed by coverage_output.glsl.
enum class CoverageBlendOutput : uint8_t {
  kSourceTimesCoverage = 0,
  kCoverage = 1,
};

struct FixedFunctionCoverageBlend {
  BlendState blend_state;
  CoverageBlendOutput output;
};

// These indices select shader specializations in both destination-read paths.
enum class CoverageBlendProgram : uint8_t {
  kSrc,
  kCount,
};

struct ProgrammableCoverageBlend {
  enum class DestinationRead { kFramebufferFetch, kOffscreen };

  CoverageBlendProgram program;
  DestinationRead destination_read;
};

using CoverageBlendPlan =
    std::variant<FixedFunctionCoverageBlend, ProgrammableCoverageBlend>;

bool SupportsCoverageBlend(BlendMode mode);

// Source alpha includes paint and inherited opacity, without coverage.
std::optional<CoverageBlendPlan> ResolveCoverageBlendPlan(
    BlendMode mode,
    Scalar source_alpha,
    bool framebuffer_fetch);

bool CanFetchCoverageBlendDestination(const Capabilities& device,
                                      const RenderTarget& target);

// The offscreen blend needs the previous color as a texture. On targets that
// flip between two resolves, the final result must also return to the caller's
// original texture.
bool NeedsCoverageReadbackTarget(const Capabilities& device,
                                 const RenderTarget& target,
                                 bool is_onscreen);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_
