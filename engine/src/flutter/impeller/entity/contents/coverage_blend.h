// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_

#include <variant>

#include "impeller/core/formats.h"
#include "impeller/geometry/color.h"

namespace impeller {

class Capabilities;
class RenderTarget;

// Coverage supplied by the shader separately from the material's source color.
enum class FragmentCoverage { kNone, kScalar };

// The scalar-coverage shaders use 0 and 1 in coverage_output.glsl.
enum class CoverageBlendOutput : uint8_t {
  kSourceTimesCoverage = 0,
  kCoverage = 1,
  kSource,
};

struct FixedFunctionBlend {
  BlendState blend_state;
  CoverageBlendOutput output;
};

// These indices select shader specializations in both destination-read paths.
enum class CoverageBlendProgram : uint8_t {
  kSrc,
  kCount,
};

struct ProgrammableBlend {
  enum class DestinationRead { kFramebufferFetch, kOffscreen };

  CoverageBlendProgram program;
  DestinationRead destination_read;
};

using BlendPlan = std::variant<FixedFunctionBlend, ProgrammableBlend>;

struct BlendCapabilities {
  bool framebuffer_fetch = false;
};

bool SupportsCoverageBlend(BlendMode mode);

// Scalar coverage requires SupportsCoverageBlend. Source opacity accounts for
// the material, filters and inherited opacity before applying coverage.
BlendPlan ResolveBlendPlan(BlendMode mode,
                           bool source_is_opaque,
                           FragmentCoverage coverage,
                           const BlendCapabilities& capabilities);

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
