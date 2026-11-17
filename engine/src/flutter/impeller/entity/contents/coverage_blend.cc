// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/coverage_blend.h"

#include "impeller/core/texture.h"
#include "impeller/renderer/capabilities.h"
#include "impeller/renderer/render_target.h"

namespace impeller {

namespace {

struct CoverageBlendRule {
  std::optional<FixedFunctionCoverageBlend> (*fixed_function)(Scalar);
  std::optional<CoverageBlendProgram> program;
};

std::optional<CoverageBlendRule> GetCoverageBlendRule(BlendMode mode) {
  switch (mode) {
    case BlendMode::kSrc:
      return CoverageBlendRule{
          .fixed_function =
              [](Scalar alpha) -> std::optional<FixedFunctionCoverageBlend> {
            if (alpha == 1.0f) {
              return FixedFunctionCoverageBlend{
                  BlendState::Add(BlendFactor::kOne,
                                  BlendFactor::kOneMinusSourceAlpha),
                  CoverageBlendOutput::kSourceTimesCoverage};
            }
            if (alpha == 0.0f) {
              return FixedFunctionCoverageBlend{
                  BlendState::Add(BlendFactor::kZero,
                                  BlendFactor::kOneMinusSourceAlpha),
                  CoverageBlendOutput::kCoverage};
            }
            return std::nullopt;
          },
          .program = CoverageBlendProgram::kSrc};
    default:
      return std::nullopt;
  }
}

}  // namespace

bool SupportsCoverageBlend(BlendMode mode) {
  return GetCoverageBlendRule(mode).has_value();
}

std::optional<CoverageBlendPlan> ResolveCoverageBlendPlan(
    BlendMode mode,
    Scalar source_alpha,
    bool framebuffer_fetch) {
  const auto rule = GetCoverageBlendRule(mode);
  if (!rule) {
    return std::nullopt;
  }
  if (auto blend = rule->fixed_function(source_alpha)) {
    return *blend;
  }
  if (!rule->program) {
    return std::nullopt;
  }

  using Read = ProgrammableCoverageBlend::DestinationRead;
  return ProgrammableCoverageBlend{*rule->program, framebuffer_fetch
                                                       ? Read::kFramebufferFetch
                                                       : Read::kOffscreen};
}

bool CanFetchCoverageBlendDestination(const Capabilities& device,
                                      const RenderTarget& target) {
  return target.GetMaxColorAttachmentBindIndex() == 0 &&
         device.SupportsFramebufferFetch();
}

bool NeedsCoverageReadbackTarget(const Capabilities& device,
                                 const RenderTarget& target,
                                 bool is_onscreen) {
  const auto texture = target.GetRenderTargetTexture();
  return is_onscreen || !texture ||
         texture->GetTextureDescriptor().storage_mode ==
             StorageMode::kDeviceTransient ||
         !(texture->GetTextureDescriptor().usage & TextureUsage::kShaderRead) ||
         (target.GetSampleCount() != SampleCount::kCount1 &&
          !device.SupportsReadFromResolve());
}

}  // namespace impeller
