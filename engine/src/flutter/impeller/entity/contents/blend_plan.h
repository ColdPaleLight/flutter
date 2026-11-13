// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_BLEND_PLAN_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_BLEND_PLAN_H_

#include <cstdint>
#include <optional>

#include "impeller/core/formats.h"
#include "impeller/geometry/color.h"
#include "impeller/geometry/point.h"

namespace impeller {

// Resolves a blend mode for a draw whose fragment shader emits its source
// color.
ColorAttachmentDescriptor ResolveBlendState(
    BlendMode mode,
    bool is_for_rrect_blur_clear = false);

struct BlendPlan {
  // Matches the output expressions in coverage_blend.glsl. S is the
  // premultiplied source before applying fragment coverage c.
  enum class Output : uint8_t {
    kCoveredSource = 0,
    kCoverage = 1,
    kCompositedColor = 2,
    kNone = 3,  // Unused output slot.
  };
  enum class DestinationRead { kNone, kFramebufferFetch, kTextureCopy };
  enum class ShaderVariant { kSingleSource, kDualSource, kFetch, kBackdrop };

  ColorAttachmentDescriptor blend_state;
  Output primary_output = Output::kCoveredSource;
  Output secondary_output = Output::kNone;
  DestinationRead destination_read = DestinationRead::kNone;

  ShaderVariant GetShaderVariant() const;
  static Vector2 EncodeOutputs(const BlendPlan* plan);
};

struct BlendPassCapabilities {
  bool dual_source = false;
  bool framebuffer_fetch = false;
  bool texture_copy = false;
};

enum class BlendSourceAlpha { kTransparent, kOpaque, kUnknown };

BlendSourceAlpha ClassifyBlendSourceAlpha(Scalar alpha);

constexpr bool SupportsCoverageBlend(BlendMode mode) {
  return mode == BlendMode::kSrc;
}

// Resolves a draw that supplies independent fragment coverage. Source alpha
// includes paint and inherited opacity. It excludes coverage.
std::optional<BlendPlan> ResolveCoverageBlendPlan(BlendMode mode,
                                                  BlendSourceAlpha source_alpha,
                                                  BlendPassCapabilities pass);

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_BLEND_PLAN_H_
