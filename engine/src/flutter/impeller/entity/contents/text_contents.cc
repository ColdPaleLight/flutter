// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/text_contents.h"

#include <array>
#include <optional>
#include <utility>

#include "impeller/core/buffer_view.h"
#include "impeller/core/formats.h"
#include "impeller/core/sampler_descriptor.h"
#include "impeller/entity/contents/coverage_blend.h"
#include "impeller/entity/entity.h"
#include "impeller/geometry/color.h"
#include "impeller/geometry/point.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/typographer/glyph_atlas.h"

namespace impeller {
namespace {

// On Linux we use FreeType and on Windows we use DirectWrite/GDI to rasterize
// glyphs. Because we render in linear space, light text on a dark background
// would look too thin without correction. To compensate, we calculate a
// contrast/gamma correction factor based on the text color's luminance, which
// is used in the shader to adjust the glyph's coverage.
constexpr bool kPlatformGammaCorrectionDefault =
#if FML_OS_LINUX || FML_OS_WIN
    true;
#else
    false;
#endif

// The contrast/gamma exponent applied in the shader ranges from 1.0 for black
// text to 1.0 + kMaxGammaCorrection for white text. This interpolates the
// exponent based on the text color's luminance. On Linux, 1.2 equates to a
// maximum 2.2 sRGB gamma. On Windows, DirectWrite and GDI employ higher base
// gamma and contrast enhancement, so 1.6 is used to match Skia's perceived
// visual weight and edge sharpness.
constexpr Scalar kMaxGammaCorrection =
#if FML_OS_WIN
    1.6f;
#else
    1.2f;
#endif

Point SizeToPoint(Size size) {
  return Point(size.width, size.height);
}
}  // namespace

using VS = GlyphAtlasPipeline::VertexShader;

TextContents::TextContents() {}

TextContents::~TextContents() = default;

void TextContents::SetTextFrame(const std::shared_ptr<TextFrame>& frame) {
  frame_ = frame;
}

void TextContents::SetColor(Color color) {
  color_ = color;
}

void TextContents::SetUsesGlyphCoverageForBlend(bool enabled) {
  uses_glyph_coverage_for_blend_ = enabled;
}

Color TextContents::GetColor() const {
  return color_.WithAlpha(color_.alpha * inherited_opacity_);
}

void TextContents::SetInheritedOpacity(Scalar opacity) {
  inherited_opacity_ = opacity;
}

void TextContents::SetPosition(Point position) {
  position_ = position;
}

void TextContents::SetScreenTransform(const Matrix& transform) {
  screen_transform_ = transform;
}

void TextContents::SetForceTextColor(bool value) {
  force_text_color_ = value;
}

std::optional<Rect> TextContents::GetCoverage(const Entity& entity) const {
  return frame_->GetBounds().TransformBounds(entity.GetTransform());
}

void TextContents::SetTextProperties(
    Color color,
    const std::optional<StrokeParameters>& stroke) {
  if (frame_->HasColor()) {
    // Alpha is always applied when rendering, remove it here so
    // we do not double-apply the alpha.
    properties_.tone_or_color = color.WithAlpha(1.0);
  } else {
    properties_.tone_or_color = GlyphProperties::ComputeTone(color);
  }
  properties_.stroke = stroke;
}

namespace {
Scalar AttractToOne(Scalar x) {
  // Epsilon was decided by looking at the floating point inaccuracies in
  // the ScaledK test.
  const Scalar epsilon = 0.005f;
  if (std::abs(x - 1.f) < epsilon) {
    return 1.f;
  }
  if (std::abs(x + 1.f) < epsilon) {
    return -1.f;
  }
  return x;
}

}  // namespace

size_t TextContents::ComputeVertexData(
    VS::PerVertexData* vtx_contents,
    const Matrix& entity_offset_transform,
    const std::shared_ptr<TextFrame>& frame,
    Point position,
    const Matrix& screen_transform,
    GlyphProperties glyph_properties,
    const std::shared_ptr<GlyphAtlas>& atlas) {
  // Common vertex information for all glyphs.
  // All glyphs are given the same vertex information in the form of a
  // unit-sized quad. The size of the glyph is specified in per instance data
  // and the vertex shader uses this to size the glyph correctly. The
  // interpolated vertex information is also used in the fragment shader to
  // sample from the glyph atlas.

  constexpr std::array<Point, 4> unit_points = {Point{0, 0}, Point{1, 0},
                                                Point{0, 1}, Point{1, 1}};

  ISize atlas_size = atlas->GetTexture()->GetSize();
  bool is_translation_scale = entity_offset_transform.IsTranslationScaleOnly();
  Matrix basis_transform = entity_offset_transform.Basis();

  VS::PerVertexData vtx;
  size_t i = 0u;

  const Matrix frame_transform =
      screen_transform * Matrix::MakeTranslation(position);
  Rational rounded_scale =
      TextFrame::RoundScaledFontSize(frame_transform.GetMaxBasisLengthXY());
  Scalar inverted_rounded_scale = static_cast<Scalar>(rounded_scale.Invert());
  Matrix unscaled_basis =
      basis_transform *
      Matrix::MakeScale({inverted_rounded_scale, inverted_rounded_scale, 1});

  // In typical scales < 48x these values should be -1 or 1. We round to
  // those to avoid inaccuracies.
  unscaled_basis.m[0] = AttractToOne(unscaled_basis.m[0]);
  unscaled_basis.m[5] = AttractToOne(unscaled_basis.m[5]);

  // Compute the device origin of the entire frame.
  Point screen_offset = (entity_offset_transform * Point(0, 0));

  for (const TextRun& run : frame->GetRuns()) {
    const Font& font = run.GetFont();
    const ScaledFont scaled_font{.font = font, .scale = rounded_scale};
    const FontGlyphAtlas* font_atlas = atlas->GetFontGlyphAtlas(scaled_font);

    if (!font_atlas) {
      VALIDATION_LOG << "Could not find font in the atlas.";
      // We will not find glyph bounds data for any characters in this run.
      break;
    }

    // Adjust glyph position based on the subpixel rounding used by the font.
    //
    // This value is really only used in the is_translation_scale case below,
    // but that usage appears inside a pair of nested loops so we compute it
    // once here for the common case for use many times below.
    // For the other case, this is a fairly quick computation if we are
    // only doing it just once.
    Point subpixel_adjustment(0.5, 0.5);
    switch (font.GetAxisAlignment()) {
      case AxisAlignment::kNone:
        break;
      case AxisAlignment::kX:
        subpixel_adjustment.x = 0.125;
        break;
      case AxisAlignment::kY:
        subpixel_adjustment.y = 0.125;
        break;
      case AxisAlignment::kAll:
        subpixel_adjustment.x = 0.125;
        subpixel_adjustment.y = 0.125;
        break;
    }

    for (const TextRun::GlyphPosition& glyph_position :
         run.GetGlyphPositions()) {
      SubpixelPosition subpixel = TextFrame::ComputeSubpixelPosition(
          glyph_position, font.GetAxisAlignment(), frame_transform);
      SubpixelGlyph subpixel_glyph(glyph_position.glyph, subpixel,
                                   glyph_properties);
      FrameBounds frame_bounds =
          font_atlas->FindGlyphBounds(subpixel_glyph).value_or(FrameBounds{});

      // If frame_bounds.is_placeholder is true, either this set of attributes
      // were not captured by the FirstPass dispatcher or this is the first
      // frame the glyph has been rendered and so its atlas position was not
      // known when the glyph was recorded. Perform a slow lookup into the
      // glyph atlas hash table.
      if (frame_bounds.is_placeholder) {
        VALIDATION_LOG << "Frame bounds are not present in the atlas "
                       << font_atlas;
        continue;
      }

      // For each glyph, we compute two rectangles. One for the vertex
      // positions and one for the texture coordinates (UVs). The atlas
      // glyph bounds are used to compute UVs in cases where the
      // destination and source sizes may differ due to clamping the sizes
      // of large glyphs.
      Point uv_origin = frame_bounds.atlas_bounds.GetLeftTop() / atlas_size;
      Point uv_size =
          SizeToPoint(frame_bounds.atlas_bounds.GetSize()) / atlas_size;

      for (const Point& point : unit_points) {
        Point position;
        if (is_translation_scale) {
          Point unrounded_glyph_position =
              // This is for RTL text.
              unscaled_basis * frame_bounds.glyph_bounds.GetLeftTop() +
              (basis_transform * glyph_position.position);

          Point screen_glyph_position =
              (screen_offset + unrounded_glyph_position + subpixel_adjustment)
                  .Floor();
          position =
              (screen_glyph_position +
               (unscaled_basis * point * frame_bounds.glyph_bounds.GetSize()))
                  .Round();
        } else {
          Rect scaled_bounds =
              frame_bounds.glyph_bounds.Scale(inverted_rounded_scale);
          position = entity_offset_transform *
                     (glyph_position.position + scaled_bounds.GetLeftTop() +
                      point * scaled_bounds.GetSize());
        }
        vtx.uv = uv_origin + (uv_size * point);
        vtx.position = position;
        vtx_contents[i++] = vtx;
      }
    }
  }
  return i;
}

bool TextContents::Render(const ContentContext& renderer,
                          const Entity& entity,
                          RenderPass& pass) const {
  return RenderGlyphs(renderer, entity, pass, nullptr);
}

std::optional<BlendSourceAlpha> TextContents::GetCoverageSourceAlpha() const {
  if (uses_glyph_coverage_for_blend_) {
    FML_DCHECK(frame_->GetAtlasType() == GlyphAtlas::Type::kAlphaBitmap);
    return ClassifyBlendSourceAlpha(GetColor().alpha);
  }
  return std::nullopt;
}

bool TextContents::RenderWithCoverageBlend(const ContentContext& renderer,
                                           const Entity& entity,
                                           RenderPass& pass,
                                           const CoverageBlend& blend) const {
  return RenderGlyphs(renderer, entity, pass, &blend);
}

bool TextContents::RenderGlyphs(const ContentContext& renderer,
                                const Entity& entity,
                                RenderPass& pass,
                                const CoverageBlend* blend) const {
  const Color color = GetColor();
  const auto type = frame_->GetAtlasType();
  if (color.IsTransparent() && !blend) {
    return true;
  }
  using DestinationRead = BlendPlan::DestinationRead;
  using Variant = BlendPlan::ShaderVariant;
  const auto variant =
      blend ? blend->plan.GetShaderVariant() : Variant::kSingleSource;

  const std::shared_ptr<GlyphAtlas>& atlas =
      renderer.GetLazyGlyphAtlas()->CreateOrGetGlyphAtlas(
          *renderer.GetContext(), renderer.GetTransientsDataBuffer(), type);

  if (!atlas || !atlas->IsValid()) {
    VALIDATION_LOG << "Cannot render glyphs without prepared atlas.";
    return false;
  }

  const Matrix& entity_transform = entity.GetTransform();
  const bool is_translation_scale = entity_transform.IsTranslationScaleOnly();
  Scalar text_contrast = 1.0f;
  if (frame_->GetEnableGammaCorrection().value_or(
          kPlatformGammaCorrectionDefault)) {
    // Calculate relative luminance using Rec. 709 luma coefficients.
    Scalar luma =
        color.red * 0.2126f + color.green * 0.7152f + color.blue * 0.0722f;
    text_contrast = 1.0f + luma * kMaxGammaCorrection;
  }

  SamplerDescriptor sampler_desc;
  if (is_translation_scale) {
    // When the transform is translation+scale only, we normally use nearest-
    // neighbor sampling for pixel-perfect text. However, if the X and Y
    // scales differ significantly (non-uniform / anisotropic scaling, e.g.
    // Transform.scale(scaleY: 2)), the glyph atlas entry is rasterized at
    // max(|scaleX|,|scaleY|) uniformly and the compensating unscaled_basis
    // squeezes one axis, causing a minification. Nearest-neighbor during
    // minification discards texel columns/rows, producing jagged diagonals
    // and varying stroke weights. Fall back to bilinear in that case.
    // See https://github.com/flutter/flutter/issues/182143
    constexpr Scalar kMinScaleForRatio = 0.001f;
    constexpr Scalar kAnisotropicScaleThreshold = 1.15f;
    const Scalar sx = entity_transform.GetBasisX().GetLength();
    const Scalar sy = entity_transform.GetBasisY().GetLength();
    const Scalar ratio = (sx > sy) ? sx / std::max(sy, kMinScaleForRatio)
                                   : sy / std::max(sx, kMinScaleForRatio);
    if (ratio > kAnisotropicScaleThreshold) {
      // Non-uniform scale — use bilinear to avoid aliasing.
      sampler_desc.min_filter = MinMagFilter::kLinear;
      sampler_desc.mag_filter = MinMagFilter::kLinear;
    } else {
      sampler_desc.min_filter = MinMagFilter::kNearest;
      sampler_desc.mag_filter = MinMagFilter::kNearest;
    }
  } else {
    // Currently, we only propagate the scale of the transform to the atlas
    // renderer, so if the transform has more than just a translation, we turn
    // on linear sampling to prevent crunchiness caused by the pixel grid not
    // being perfectly aligned.
    // The downside is that this slightly over-blurs rotated/skewed text.
    sampler_desc.min_filter = MinMagFilter::kLinear;
    sampler_desc.mag_filter = MinMagFilter::kLinear;
  }

  // No mipmaps for glyph atlas (glyphs are generated at exact scales).
  sampler_desc.mip_filter = MipFilter::kBase;

  const auto sampler =
      renderer.GetContext()->GetSamplerLibrary()->GetSampler(sampler_desc);

  HostBuffer& data_host_buffer = renderer.GetTransientsDataBuffer();
  HostBuffer& indexes_host_buffer = renderer.GetTransientsIndexesBuffer();
  size_t glyph_count = 0;
  for (const auto& run : frame_->GetRuns()) {
    glyph_count += run.GetGlyphPositions().size();
  }
  // Fixed-function blending can draw the entire frame. A destination read
  // must see preceding glyphs, so split groups when their quads intersect.
  std::vector<std::pair<size_t, Rect>> groups;
  Rect group_bounds;
  BufferView buffer_view = data_host_buffer.Emplace(
      glyph_count * 4 * sizeof(VS::PerVertexData), alignof(VS::PerVertexData),
      [&](uint8_t* data) {
        auto* vertices = reinterpret_cast<VS::PerVertexData*>(data);
        glyph_count =
            ComputeVertexData(vertices, entity_transform, frame_, position_,
                              screen_transform_, properties_, atlas) /
            4;
        if (!blend || blend->plan.destination_read == DestinationRead::kNone) {
          return;
        }
        for (size_t i = 0; i < glyph_count; i++) {
          const auto* quad = vertices + 4 * i;
          const Rect bounds =
              Rect::MakePointBounds(
                  std::array{quad[0].position, quad[1].position,
                             quad[2].position, quad[3].position})
                  .value();
          if (bounds.IntersectsWithRect(group_bounds)) {
            groups.emplace_back(i, group_bounds);
            group_bounds = bounds;
          } else {
            group_bounds = group_bounds.Union(bounds);
          }
        }
      });
  if (glyph_count == 0) {
    return true;
  }

  const auto draw_group = [&](size_t first, size_t end, const Rect& bounds) {
    const size_t index_count = (end - first) * 6;
    auto indices = indexes_host_buffer.Emplace(
        index_count * sizeof(uint16_t), alignof(uint16_t), [&](uint8_t* data) {
          auto* out = reinterpret_cast<uint16_t*>(data);
          for (size_t i = first; i < end; i++) {
            const size_t base = i * 4;
            *out++ = base;
            *out++ = base + 1;
            *out++ = base + 2;
            *out++ = base + 1;
            *out++ = base + 2;
            *out++ = base + 3;
          }
        });
    const auto draw_with_shader = [&]<class PipelineT>(
                                      RenderPass& target,
                                      const BlendBackdrop* backdrop) {
      using Shader = typename PipelineT::FragmentShader;
      typename Shader::FragInfo frag_info;
      frag_info.use_text_color = force_text_color_ ? 1.0f : 0.0f;
      frag_info.text_color = ToVector(color.Premultiply());
      frag_info.is_color_glyph = type == GlyphAtlas::Type::kColorBitmap;
      frag_info.blend_outputs =
          BlendPlan::EncodeOutputs(blend ? &blend->plan : nullptr);
      frag_info.text_contrast = text_contrast;
      VS::FrameInfo frame_info;
      frame_info.mvp = Entity::GetShaderTransform(entity.GetShaderClipDepth(),
                                                  target, Matrix());

      auto options = OptionsFromPassAndEntity(target, entity);
      options.primitive_type = PrimitiveType::kTriangle;
      if (blend) {
        options.blend_state = blend->plan.blend_state;
      }
      target.SetPipeline(renderer.GetGlyphAtlasPipeline(options, variant));
      target.SetCommandLabel("TextFrame");
      VS::BindFrameInfo(target, data_host_buffer.EmplaceUniform(frame_info));
      Shader::BindFragInfo(target, data_host_buffer.EmplaceUniform(frag_info));
      Shader::BindGlyphAtlasSampler(target, atlas->GetTexture(), sampler);
      if (!BlendBackdrop::BindIfUsed<Shader>(target, data_host_buffer,
                                             backdrop)) {
        return false;
      }
      target.SetVertexBuffer(buffer_view);
      target.SetIndexBuffer(indices, IndexType::k16bit);
      target.SetElementCount(index_count);
      return target.Draw().ok();
    };
    const auto draw = [&](RenderPass& target, const BlendBackdrop* backdrop) {
      return GlyphAtlasShaderFamily::Dispatch(
          variant, [&]<typename Pipeline>() {
            return draw_with_shader.template operator()<Pipeline>(target,
                                                                  backdrop);
          });
    };
    return blend ? blend->Draw(
                       pass, [&] { return bounds; }, draw)
                 : draw(pass, nullptr);
  };

  size_t first = 0;
  for (const auto& [end, bounds] : groups) {
    if (!draw_group(first, end, bounds)) {
      return false;
    }
    first = end;
  }
  return draw_group(first, glyph_count, group_bounds);
}

}  // namespace impeller
