// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/coverage_blend.h"

#include "impeller/core/texture.h"
#include "impeller/renderer/capabilities.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/renderer/render_target.h"

namespace impeller {

BlendPassCapabilities GetBlendPassCapabilities(const Capabilities& device,
                                               const RenderTarget& target,
                                               bool can_copy_backdrop) {
  const bool single_color_target = target.HasColorAttachment(0) &&
                                   target.GetMaxColorAttachmentBindIndex() == 0;
  return {
      .dual_source = single_color_target && device.SupportsDualSourceBlending(),
      .framebuffer_fetch = single_color_target &&
                           target.GetSampleCount() == SampleCount::kCount1 &&
                           device.SupportsFramebufferFetch(),
      .texture_copy = single_color_target && can_copy_backdrop,
  };
}

bool CanSampleBlendBackdrop(const RenderTarget& target) {
  const auto texture = target.GetRenderTargetTexture();
  return texture &&
         texture->GetTextureDescriptor().storage_mode !=
             StorageMode::kDeviceTransient &&
         (texture->GetTextureDescriptor().usage & TextureUsage::kShaderRead);
}

bool NeedsCoverageReadbackTarget(const Capabilities& device,
                                 const RenderTarget& target,
                                 bool is_onscreen) {
  return is_onscreen || !CanSampleBlendBackdrop(target) ||
         (target.GetSampleCount() != SampleCount::kCount1 &&
          !device.SupportsReadFromResolve());
}

Vector4 BlendBackdrop::GetUVTransform() const {
  const Vector2 scale(1.0f / bounds.GetWidth(), 1.0f / bounds.GetHeight());
  return {scale.x, scale.y, -bounds.GetLeft() * scale.x,
          -bounds.GetTop() * scale.y};
}

bool CoverageBlend::Draw(
    RenderPass& pass,
    absl::FunctionRef<std::optional<Rect>()> get_bounds,
    absl::FunctionRef<bool(RenderPass&, const BlendBackdrop*)> draw) const {
  if (plan.destination_read != BlendPlan::DestinationRead::kTextureCopy) {
    return draw(pass, nullptr);
  }
  const auto bounds = get_bounds();
  return !bounds ||
         prepare_backdrop(
             *bounds, [&](RenderPass& target, const BlendBackdrop& backdrop) {
               return draw(target, &backdrop);
             });
}

}  // namespace impeller
