// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_PIPELINES_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_PIPELINES_H_

#include "flutter/fml/build_config.h"
#include "impeller/entity/advanced_blend.frag.h"
#include "impeller/entity/advanced_blend.vert.h"
#include "impeller/entity/circle.frag.h"
#include "impeller/entity/circle.vert.h"
#include "impeller/entity/circle_backdrop.frag.h"
#include "impeller/entity/circle_dual.frag.h"
#include "impeller/entity/circle_fetch.frag.h"
#include "impeller/entity/clip.frag.h"
#include "impeller/entity/clip.vert.h"
#include "impeller/entity/color_matrix_color_filter.frag.h"
#include "impeller/entity/complex_rse.frag.h"
#include "impeller/entity/conical_gradient_fill_conical.frag.h"
#include "impeller/entity/conical_gradient_fill_radial.frag.h"
#include "impeller/entity/conical_gradient_fill_strip.frag.h"
#include "impeller/entity/conical_gradient_fill_strip_radial.frag.h"
#include "impeller/entity/conical_gradient_ssbo_fill.frag.h"
#include "impeller/entity/conical_gradient_uniform_fill_conical.frag.h"
#include "impeller/entity/conical_gradient_uniform_fill_radial.frag.h"
#include "impeller/entity/conical_gradient_uniform_fill_strip.frag.h"
#include "impeller/entity/conical_gradient_uniform_fill_strip_radial.frag.h"
#include "impeller/entity/contents/blend_plan.h"
#include "impeller/entity/fast_gradient.frag.h"
#include "impeller/entity/fast_gradient.vert.h"
#include "impeller/entity/filter_position.vert.h"
#include "impeller/entity/filter_position_uv.vert.h"
#include "impeller/entity/framebuffer_blend.frag.h"
#include "impeller/entity/framebuffer_blend.vert.h"
#include "impeller/entity/gaussian.frag.h"
#include "impeller/entity/glyph_atlas.frag.h"
#include "impeller/entity/glyph_atlas.vert.h"
#include "impeller/entity/glyph_atlas_backdrop.frag.h"
#include "impeller/entity/glyph_atlas_dual.frag.h"
#include "impeller/entity/glyph_atlas_fetch.frag.h"
#include "impeller/entity/gradient_fill.vert.h"
#include "impeller/entity/linear_gradient_fill.frag.h"
#include "impeller/entity/linear_gradient_ssbo_fill.frag.h"
#include "impeller/entity/linear_gradient_uniform_fill.frag.h"
#include "impeller/entity/linear_to_srgb_filter.frag.h"
#include "impeller/entity/morphology_filter.frag.h"
#include "impeller/entity/porter_duff_blend.frag.h"
#include "impeller/entity/porter_duff_blend.vert.h"
#include "impeller/entity/radial_gradient_fill.frag.h"
#include "impeller/entity/radial_gradient_ssbo_fill.frag.h"
#include "impeller/entity/radial_gradient_uniform_fill.frag.h"
#include "impeller/entity/rrect_blur.frag.h"
#include "impeller/entity/rrect_like_blur.vert.h"
#include "impeller/entity/rsuperellipse_blur.frag.h"
#include "impeller/entity/shadow_vertices.frag.h"
#include "impeller/entity/shadow_vertices.vert.h"
#include "impeller/entity/solid_fill.frag.h"
#include "impeller/entity/solid_fill.vert.h"
#include "impeller/entity/srgb_to_linear_filter.frag.h"
#include "impeller/entity/sweep_gradient_fill.frag.h"
#include "impeller/entity/sweep_gradient_ssbo_fill.frag.h"
#include "impeller/entity/sweep_gradient_uniform_fill.frag.h"
#include "impeller/entity/texture_downsample.frag.h"
#include "impeller/entity/texture_downsample_bounded.frag.h"
#include "impeller/entity/texture_fill.frag.h"
#include "impeller/entity/texture_fill.vert.h"
#include "impeller/entity/texture_fill_strict_src.frag.h"
#include "impeller/entity/texture_uv_fill.vert.h"
#include "impeller/entity/tiled_texture_fill.frag.h"
#include "impeller/entity/uber_sdf.frag.h"
#include "impeller/entity/uber_sdf_backdrop.frag.h"
#include "impeller/entity/uber_sdf_dual.frag.h"
#include "impeller/entity/uber_sdf_fetch.frag.h"
#include "impeller/entity/uber_sdf_ssbo.frag.h"
#include "impeller/entity/vertices_uber_1.frag.h"
#include "impeller/entity/vertices_uber_2.frag.h"
#include "impeller/entity/yuv_to_rgb_filter.frag.h"
#include "impeller/renderer/pipeline.h"

#ifdef IMPELLER_ENABLE_OPENGLES
#include "impeller/entity/texture_downsample_gles.frag.h"
#include "impeller/entity/tiled_texture_fill_external.frag.h"
#endif  // IMPELLER_ENABLE_OPENGLES

// TODO(gaaclarke): These should be split up into different files.
namespace impeller {

template <typename T>
using GradientPipelineHandle =
    RenderPipelineHandle<GradientFillVertexShader, T>;

using AdvancedBlendPipelineHandle =
    RenderPipelineHandle<AdvancedBlendVertexShader,
                         AdvancedBlendFragmentShader>;

using FramebufferBlendPipelineHandle =
    RenderPipelineHandle<FramebufferBlendVertexShader,
                         FramebufferBlendFragmentShader>;

// clang-format off
using BlendColorBurnPipeline = AdvancedBlendPipelineHandle;
using BlendColorDodgePipeline = AdvancedBlendPipelineHandle;
using BlendColorPipeline = AdvancedBlendPipelineHandle;
using BlendDarkenPipeline = AdvancedBlendPipelineHandle;
using BlendDifferencePipeline = AdvancedBlendPipelineHandle;
using BlendExclusionPipeline = AdvancedBlendPipelineHandle;
using BlendHardLightPipeline = AdvancedBlendPipelineHandle;
using BlendHuePipeline = AdvancedBlendPipelineHandle;
using BlendLightenPipeline = AdvancedBlendPipelineHandle;
using BlendLuminosityPipeline = AdvancedBlendPipelineHandle;
using BlendMultiplyPipeline = AdvancedBlendPipelineHandle;
using BlendOverlayPipeline = AdvancedBlendPipelineHandle;
using BlendSaturationPipeline = AdvancedBlendPipelineHandle;
using BlendScreenPipeline = AdvancedBlendPipelineHandle;
using BlendSoftLightPipeline = AdvancedBlendPipelineHandle;
using CirclePipeline = RenderPipelineHandle<CircleVertexShader, CircleFragmentShader>;
using CircleDualPipeline = RenderPipelineHandle<CircleVertexShader, CircleDualFragmentShader>;
using CircleFetchPipeline = RenderPipelineHandle<CircleVertexShader, CircleFetchFragmentShader>;
using CircleBackdropPipeline = RenderPipelineHandle<CircleVertexShader, CircleBackdropFragmentShader>;
using ClipPipeline = RenderPipelineHandle<ClipVertexShader, ClipFragmentShader>;
using ColorMatrixColorFilterPipeline = RenderPipelineHandle<FilterPositionUvVertexShader, ColorMatrixColorFilterFragmentShader>;
using ConicalGradientFillConicalPipeline = GradientPipelineHandle<ConicalGradientFillConicalFragmentShader>;
using ConicalGradientFillRadialPipeline = GradientPipelineHandle<ConicalGradientFillRadialFragmentShader>;
using ConicalGradientFillStripPipeline = GradientPipelineHandle<ConicalGradientFillStripFragmentShader>;
using ConicalGradientFillStripRadialPipeline = GradientPipelineHandle<ConicalGradientFillStripRadialFragmentShader>;
using ConicalGradientSSBOFillPipeline = GradientPipelineHandle<ConicalGradientSsboFillFragmentShader>;
using ConicalGradientUniformFillConicalPipeline = GradientPipelineHandle<ConicalGradientUniformFillConicalFragmentShader>;
using ConicalGradientUniformFillRadialPipeline = GradientPipelineHandle<ConicalGradientUniformFillRadialFragmentShader>;
using ConicalGradientUniformFillStripPipeline = GradientPipelineHandle<ConicalGradientUniformFillStripFragmentShader>;
using ConicalGradientUniformFillStripRadialPipeline = GradientPipelineHandle<ConicalGradientUniformFillStripRadialFragmentShader>;
using FastGradientPipeline = RenderPipelineHandle<FastGradientVertexShader, FastGradientFragmentShader>;
using FramebufferBlendColorBurnPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendColorDodgePipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendColorPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendDarkenPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendDifferencePipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendExclusionPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendHardLightPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendHuePipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendLightenPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendLuminosityPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendMultiplyPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendOverlayPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendSaturationPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendScreenPipeline = FramebufferBlendPipelineHandle;
using FramebufferBlendSoftLightPipeline = FramebufferBlendPipelineHandle;
using GaussianBlurPipeline = RenderPipelineHandle<FilterPositionUvVertexShader, GaussianFragmentShader>;
using GlyphAtlasPipeline = RenderPipelineHandle<GlyphAtlasVertexShader, GlyphAtlasFragmentShader>;
using GlyphAtlasDualPipeline = RenderPipelineHandle<GlyphAtlasVertexShader, GlyphAtlasDualFragmentShader>;
using GlyphAtlasFetchPipeline = RenderPipelineHandle<GlyphAtlasVertexShader, GlyphAtlasFetchFragmentShader>;
using GlyphAtlasBackdropPipeline = RenderPipelineHandle<GlyphAtlasVertexShader, GlyphAtlasBackdropFragmentShader>;
using LinearGradientFillPipeline = GradientPipelineHandle<LinearGradientFillFragmentShader>;
using LinearGradientSSBOFillPipeline = GradientPipelineHandle<LinearGradientSsboFillFragmentShader>;
using LinearGradientUniformFillPipeline = GradientPipelineHandle<LinearGradientUniformFillFragmentShader>;
using LinearToSrgbFilterPipeline = RenderPipelineHandle<FilterPositionVertexShader, LinearToSrgbFilterFragmentShader>;
using MorphologyFilterPipeline = RenderPipelineHandle<FilterPositionUvVertexShader, MorphologyFilterFragmentShader>;
using PorterDuffBlendPipeline = RenderPipelineHandle<PorterDuffBlendVertexShader, PorterDuffBlendFragmentShader>;
using RadialGradientFillPipeline = GradientPipelineHandle<RadialGradientFillFragmentShader>;
using RadialGradientSSBOFillPipeline = GradientPipelineHandle<RadialGradientSsboFillFragmentShader>;
using RadialGradientUniformFillPipeline = GradientPipelineHandle<RadialGradientUniformFillFragmentShader>;
using RRectBlurPipeline = RenderPipelineHandle<RrectLikeBlurVertexShader, RrectBlurFragmentShader>;
using RSuperellipseBlurPipeline = RenderPipelineHandle<RrectLikeBlurVertexShader, RsuperellipseBlurFragmentShader>;
using ShadowVerticesShader = RenderPipelineHandle<ShadowVerticesVertexShader, ShadowVerticesFragmentShader>;
using SolidFillPipeline = RenderPipelineHandle<SolidFillVertexShader, SolidFillFragmentShader>;
using SrgbToLinearFilterPipeline = RenderPipelineHandle<FilterPositionVertexShader, SrgbToLinearFilterFragmentShader>;
using SweepGradientFillPipeline = GradientPipelineHandle<SweepGradientFillFragmentShader>;
using SweepGradientSSBOFillPipeline = GradientPipelineHandle<SweepGradientSsboFillFragmentShader>;
using SweepGradientUniformFillPipeline = GradientPipelineHandle<SweepGradientUniformFillFragmentShader>;
using TextureDownsamplePipeline = RenderPipelineHandle<TextureFillVertexShader, TextureDownsampleFragmentShader>;
using TextureDownsampleBoundedPipeline = RenderPipelineHandle<TextureFillVertexShader, TextureDownsampleBoundedFragmentShader>;
using TexturePipeline = RenderPipelineHandle<TextureFillVertexShader, TextureFillFragmentShader>;
using TextureStrictSrcPipeline = RenderPipelineHandle<TextureFillVertexShader, TextureFillStrictSrcFragmentShader>;
using TiledTexturePipeline = RenderPipelineHandle<TextureUvFillVertexShader, TiledTextureFillFragmentShader>;
using VerticesUber1Shader = RenderPipelineHandle<PorterDuffBlendVertexShader, VerticesUber1FragmentShader>;
using VerticesUber2Shader = RenderPipelineHandle<PorterDuffBlendVertexShader, VerticesUber2FragmentShader>;
using UberSDFPipeline = RenderPipelineHandle<CircleVertexShader, UberSdfFragmentShader>;
using UberSDFDualPipeline = RenderPipelineHandle<CircleVertexShader, UberSdfDualFragmentShader>;
using UberSDFFetchPipeline = RenderPipelineHandle<CircleVertexShader, UberSdfFetchFragmentShader>;
using UberSDFBackdropPipeline = RenderPipelineHandle<CircleVertexShader, UberSdfBackdropFragmentShader>;
using UberSDFSSBOPipeline = RenderPipelineHandle<CircleVertexShader, UberSdfSsboFragmentShader>;
using ComplexRSEPipeline = RenderPipelineHandle<CircleVertexShader, ComplexRseFragmentShader>;

// The same family determines pipeline caching and the typed resource bindings.
template <typename Single, typename Dual, typename Fetch, typename Backdrop>
struct CoverageShaderFamily {
  using SingleSourcePipeline = Single;
  using DualSourcePipeline = Dual;
  using FetchPipeline = Fetch;
  using BackdropPipeline = Backdrop;

  static bool Dispatch(BlendPlan::ShaderVariant variant, auto&& draw) {
    switch (variant) {
      case BlendPlan::ShaderVariant::kSingleSource:
        return draw.template operator()<Single>();
      case BlendPlan::ShaderVariant::kDualSource:
        return draw.template operator()<Dual>();
      case BlendPlan::ShaderVariant::kFetch:
        return draw.template operator()<Fetch>();
      case BlendPlan::ShaderVariant::kBackdrop:
        return draw.template operator()<Backdrop>();
    }
  }
};

using CircleShaderFamily = CoverageShaderFamily<CirclePipeline, CircleDualPipeline, CircleFetchPipeline, CircleBackdropPipeline>;
using GlyphAtlasShaderFamily = CoverageShaderFamily<GlyphAtlasPipeline, GlyphAtlasDualPipeline, GlyphAtlasFetchPipeline, GlyphAtlasBackdropPipeline>;
using UberSDFShaderFamily = CoverageShaderFamily<UberSDFPipeline, UberSDFDualPipeline, UberSDFFetchPipeline, UberSDFBackdropPipeline>;
using YUVToRGBFilterPipeline = RenderPipelineHandle<FilterPositionVertexShader, YuvToRgbFilterFragmentShader>;
// clang-format on

#ifdef IMPELLER_ENABLE_OPENGLES

// Web doesn't support external texture OpenGL extensions
#if !defined(FML_OS_EMSCRIPTEN)
using TiledTextureExternalPipeline =
    RenderPipelineHandle<TextureFillVertexShader,
                         TiledTextureFillExternalFragmentShader>;
using TiledTextureUvExternalPipeline =
    RenderPipelineHandle<TextureUvFillVertexShader,
                         TiledTextureFillExternalFragmentShader>;
#endif

using TextureDownsampleGlesPipeline =
    RenderPipelineHandle<TextureFillVertexShader,
                         TextureDownsampleGlesFragmentShader>;
#endif  // IMPELLER_ENABLE_OPENGLES
}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_PIPELINES_H_
