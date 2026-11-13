// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/contents/circle_contents.h"
#include "impeller/entity/contents/color_source_contents.h"
#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/contents/coverage_blend.h"
#include "impeller/entity/geometry/circle_geometry.h"
#include "impeller/geometry/point.h"

namespace impeller {

std::unique_ptr<CircleContents> CircleContents::Make(
    std::unique_ptr<CircleGeometry> geometry,
    Color color,
    bool stroked) {
  Scalar aa_padding = geometry->GetAntialiasPadding();
  return std::unique_ptr<CircleContents>(
      new CircleContents(std::move(geometry), color, stroked, aa_padding));
}

CircleContents::CircleContents(std::unique_ptr<CircleGeometry> geometry,
                               Color color,
                               bool stroked,
                               Scalar aa_padding)
    : geometry_(std::move(geometry)),
      color_(color),
      stroked_(stroked),
      aa_padding_(aa_padding) {}

bool CircleContents::Render(const ContentContext& renderer,
                            const Entity& entity,
                            RenderPass& pass) const {
  return RenderCircle(renderer, entity, pass, nullptr, nullptr);
}

std::optional<BlendSourceAlpha> CircleContents::GetCoverageSourceAlpha() const {
  return ClassifyBlendSourceAlpha(color_.alpha * GetOpacityFactor());
}

bool CircleContents::RenderWithCoverageBlend(const ContentContext& renderer,
                                             const Entity& entity,
                                             RenderPass& pass,
                                             const CoverageBlend& blend) const {
  return blend.Draw(
      pass, [&] { return GetCoverage(entity); },
      [&](RenderPass& target, const BlendBackdrop* backdrop) {
        return RenderCircle(renderer, entity, target, &blend.plan, backdrop);
      });
}

bool CircleContents::RenderCircle(const ContentContext& renderer,
                                  const Entity& entity,
                                  RenderPass& pass,
                                  const BlendPlan* plan,
                                  const BlendBackdrop* backdrop) const {
  using VS = CirclePipeline::VertexShader;
  using Variant = BlendPlan::ShaderVariant;
  const auto variant = plan ? plan->GetShaderVariant() : Variant::kSingleSource;
  auto& data_host_buffer = renderer.GetTransientsDataBuffer();

  return CircleShaderFamily::Dispatch(variant, [&]<typename Pipeline>() {
    using FS = typename Pipeline::FragmentShader;
    typename FS::FragInfo frag_info;
    frag_info.color = color_.WithAlpha(color_.alpha * GetOpacityFactor());
    frag_info.center = geometry_->GetCenter();
    frag_info.radius = geometry_->GetRadius();
    frag_info.stroke_width = geometry_->GetStrokeWidth();
    frag_info.aa_pixels = aa_padding_;
    frag_info.stroked = stroked_ ? 1.0f : 0.0f;
    frag_info.blend_outputs = BlendPlan::EncodeOutputs(plan);

    return DrawGeometry<VS>(
        renderer, entity, pass,
        [&](ContentContextOptions options) {
          if (plan) {
            options.blend_state = plan->blend_state;
            options.depth_write_enabled = false;
          }
          return renderer.GetCirclePipeline(options, variant);
        },
        VS::FrameInfo{},
        [&](RenderPass& pass) {
          pass.SetCommandLabel("Circle");
          return FS::BindFragInfo(pass,
                                  data_host_buffer.EmplaceUniform(frag_info)) &&
                 BlendBackdrop::BindIfUsed<FS>(pass, data_host_buffer,
                                               backdrop);
        });
  });
}

std::optional<Rect> CircleContents::GetCoverage(const Entity& entity) const {
  return geometry_->GetCoverage(entity.GetTransform());
}

const Geometry* CircleContents::GetGeometry() const {
  return geometry_.get();
}

}  // namespace impeller
