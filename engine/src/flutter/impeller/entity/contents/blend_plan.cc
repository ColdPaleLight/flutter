// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/blend_plan.h"

#include "impeller/base/validation.h"

namespace impeller {
namespace {

ColorAttachmentDescriptor Blend(BlendFactor source, BlendFactor destination) {
  return {.blending_enabled = true,
          .src_color_blend_factor = source,
          .dst_color_blend_factor = destination,
          .src_alpha_blend_factor = source,
          .dst_alpha_blend_factor = destination};
}

}  // namespace

ColorAttachmentDescriptor ResolveBlendState(BlendMode mode,
                                            bool is_for_rrect_blur_clear) {
  using enum BlendFactor;
  switch (mode) {
    case BlendMode::kClear: {
      if (!is_for_rrect_blur_clear) {
        return Blend(kZero, kZero);
      }
      auto state = Blend(kDestinationColor, kOne);
      state.color_blend_op = BlendOperation::kReverseSubtract;
      state.alpha_blend_op = BlendOperation::kReverseSubtract;
      return state;
    }
    case BlendMode::kSrc: {
      auto state = Blend(kOne, kZero);
      state.blending_enabled = false;
      return state;
    }
    case BlendMode::kDst: {
      auto state = Blend(kZero, kOne);
      state.write_mask = ColorWriteMaskBits::kNone;
      return state;
    }
    case BlendMode::kSrcOver:
      return Blend(kOne, kOneMinusSourceAlpha);
    case BlendMode::kDstOver:
      return Blend(kOneMinusDestinationAlpha, kOne);
    case BlendMode::kSrcIn:
      return Blend(kDestinationAlpha, kZero);
    case BlendMode::kDstIn:
      return Blend(kZero, kSourceAlpha);
    case BlendMode::kSrcOut:
      return Blend(kOneMinusDestinationAlpha, kZero);
    case BlendMode::kDstOut:
      return Blend(kZero, kOneMinusSourceAlpha);
    case BlendMode::kSrcATop:
      return Blend(kDestinationAlpha, kOneMinusSourceAlpha);
    case BlendMode::kDstATop:
      return Blend(kOneMinusDestinationAlpha, kSourceAlpha);
    case BlendMode::kXor:
      return Blend(kOneMinusDestinationAlpha, kOneMinusSourceAlpha);
    case BlendMode::kPlus:
      return Blend(kOne, kOne);
    case BlendMode::kModulate: {
      auto state = Blend(kZero, kSourceAlpha);
      state.dst_color_blend_factor = kSourceColor;
      return state;
    }
    default:
      VALIDATION_LOG << "Cannot use blend mode " << static_cast<int>(mode)
                     << " as a pipeline blend.";
      return Blend(kOne, kOneMinusSourceAlpha);
  }
}

BlendPlan::ShaderVariant BlendPlan::GetShaderVariant() const {
  switch (destination_read) {
    case DestinationRead::kNone:
      return secondary_output == Output::kNone ? ShaderVariant::kSingleSource
                                               : ShaderVariant::kDualSource;
    case DestinationRead::kFramebufferFetch:
      return ShaderVariant::kFetch;
    case DestinationRead::kTextureCopy:
      return ShaderVariant::kBackdrop;
  }
}

Vector2 BlendPlan::EncodeOutputs(const BlendPlan* plan) {
  const auto primary = plan ? plan->primary_output : Output::kCoveredSource;
  const auto secondary = plan ? plan->secondary_output : Output::kNone;
  return {static_cast<Scalar>(primary), static_cast<Scalar>(secondary)};
}

BlendSourceAlpha ClassifyBlendSourceAlpha(Scalar alpha) {
  if (alpha == 0.0f) {
    return BlendSourceAlpha::kTransparent;
  }
  if (alpha == 1.0f) {
    return BlendSourceAlpha::kOpaque;
  }
  return BlendSourceAlpha::kUnknown;
}

std::optional<BlendPlan> ResolveCoverageBlendPlan(BlendMode mode,
                                                  BlendSourceAlpha source_alpha,
                                                  BlendPassCapabilities pass) {
  using enum BlendFactor;
  using Output = BlendPlan::Output;
  using Read = BlendPlan::DestinationRead;
  if (!SupportsCoverageBlend(mode)) {
    return std::nullopt;
  }

  // Src with coverage is c*S + (1-c)*D. For opaque ink the main
  // output's alpha is c. For transparent ink only the D term remains.
  if (source_alpha == BlendSourceAlpha::kOpaque) {
    return BlendPlan{.blend_state = ResolveBlendState(BlendMode::kSrcOver)};
  }
  if (source_alpha == BlendSourceAlpha::kTransparent) {
    return BlendPlan{.blend_state = ResolveBlendState(BlendMode::kDstOut),
                     .primary_output = Output::kCoverage};
  }
  if (pass.dual_source) {
    return BlendPlan{.blend_state = Blend(kOne, kOneMinusSource1Alpha),
                     .secondary_output = Output::kCoverage};
  }

  if (pass.framebuffer_fetch || pass.texture_copy) {
    return BlendPlan{.blend_state = ResolveBlendState(mode),
                     .primary_output = Output::kCompositedColor,
                     .destination_read = pass.framebuffer_fetch
                                             ? Read::kFramebufferFetch
                                             : Read::kTextureCopy};
  }
  return std::nullopt;
}

}  // namespace impeller
