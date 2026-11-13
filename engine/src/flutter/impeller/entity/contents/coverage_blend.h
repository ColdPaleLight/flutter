// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_

#include <memory>
#include <optional>

#include "flutter/fml/logging.h"
#include "impeller/core/host_buffer.h"
#include "impeller/core/raw_ptr.h"
#include "impeller/entity/contents/blend_plan.h"
#include "impeller/geometry/rect.h"
#include "third_party/abseil-cpp/absl/functional/function_ref.h"

namespace impeller {

class Capabilities;
class RenderPass;
class RenderTarget;
class Sampler;
class Texture;

BlendPassCapabilities GetBlendPassCapabilities(const Capabilities& device,
                                               const RenderTarget& target,
                                               bool can_copy_backdrop);

bool CanSampleBlendBackdrop(const RenderTarget& target);

// True if coverage blending would need a replacement root target so it can
// sample previous draws or return a flipped resolve to the original target.
bool NeedsCoverageReadbackTarget(const Capabilities& device,
                                 const RenderTarget& target,
                                 bool is_onscreen);

struct BlendBackdrop {
  std::shared_ptr<Texture> texture;
  Rect bounds;
  raw_ptr<const Sampler> sampler;

  template <typename Shader>
  static bool BindIfUsed(RenderPass& pass,
                         HostBuffer& buffer,
                         const BlendBackdrop* backdrop) {
    if constexpr (requires { Shader::BindBackdropSampler; }) {
      FML_DCHECK(backdrop);
      typename Shader::BackdropInfo info;
      info.uv_transform = backdrop->GetUVTransform();
      return Shader::BindBackdropInfo(pass, buffer.EmplaceUniform(info)) &&
             Shader::BindBackdropSampler(pass, backdrop->texture,
                                         backdrop->sampler);
    }
    return true;
  }

 private:
  Vector4 GetUVTransform() const;
};

// The callbacks are used synchronously while emitting a draw.
using DrawWithBlendBackdrop =
    absl::FunctionRef<bool(RenderPass&, const BlendBackdrop&)>;
using PrepareBlendBackdrop =
    absl::FunctionRef<bool(Rect, DrawWithBlendBackdrop)>;

struct CoverageBlend {
  BlendPlan plan;
  PrepareBlendBackdrop prepare_backdrop;

  // Multiple draws may use one plan. Each destination read must see all
  // preceding draws that affect its bounds.
  bool Draw(
      RenderPass& pass,
      absl::FunctionRef<std::optional<Rect>()> get_bounds,
      absl::FunctionRef<bool(RenderPass&, const BlendBackdrop*)> draw) const;
};

}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_COVERAGE_BLEND_H_
