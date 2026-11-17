// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/coverage_blend.h"

#include "fml/logging.h"
#include "impeller/core/texture.h"
#include "impeller/renderer/capabilities.h"
#include "impeller/renderer/render_target.h"

namespace impeller {

bool SupportsCoverageBlend(BlendMode mode) {
  return mode == BlendMode::kSrc;
}

BlendPlan ResolveBlendPlan(BlendMode mode,
                           bool source_is_opaque,
                           FragmentCoverage coverage,
                           const BlendCapabilities& capabilities) {
  if (coverage == FragmentCoverage::kNone) {
    return FixedFunctionBlend{BlendState::FromBlendMode(mode),
                              CoverageBlendOutput::kSource};
  }
  using Read = ProgrammableBlend::DestinationRead;
  switch (mode) {
    case BlendMode::kSrc:
      if (source_is_opaque) {
        // The alpha of the covered source is exactly the coverage.
        return FixedFunctionBlend{
            BlendState::Add(BlendFactor::kOne,
                            BlendFactor::kOneMinusSourceAlpha),
            CoverageBlendOutput::kSourceTimesCoverage};
      }
      return ProgrammableBlend{CoverageBlendProgram::kSrc,
                               capabilities.framebuffer_fetch
                                   ? Read::kFramebufferFetch
                                   : Read::kOffscreen};
    default:
      FML_UNREACHABLE();
  }
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
