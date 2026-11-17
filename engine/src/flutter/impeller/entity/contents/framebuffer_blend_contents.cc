// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "framebuffer_blend_contents.h"

#include "impeller/entity/contents/content_context.h"
#include "impeller/renderer/render_pass.h"

namespace impeller {

namespace {

ContentContextOptions GetFramebufferBlendOptions(const RenderPass& pass) {
  auto options = OptionsFromPass(pass);
  options.primitive_type = PrimitiveType::kTriangleStrip;
  return options;
}

PipelineRef GetAdvancedBlendPipeline(const ContentContext& renderer,
                                     ContentContextOptions options,
                                     BlendMode mode) {
  options.blend_mode = BlendMode::kSrc;
  switch (mode) {
    case BlendMode::kScreen:
      return renderer.GetFramebufferBlendScreenPipeline(options);
    case BlendMode::kOverlay:
      return renderer.GetFramebufferBlendOverlayPipeline(options);
    case BlendMode::kDarken:
      return renderer.GetFramebufferBlendDarkenPipeline(options);
    case BlendMode::kLighten:
      return renderer.GetFramebufferBlendLightenPipeline(options);
    case BlendMode::kColorDodge:
      return renderer.GetFramebufferBlendColorDodgePipeline(options);
    case BlendMode::kColorBurn:
      return renderer.GetFramebufferBlendColorBurnPipeline(options);
    case BlendMode::kHardLight:
      return renderer.GetFramebufferBlendHardLightPipeline(options);
    case BlendMode::kSoftLight:
      return renderer.GetFramebufferBlendSoftLightPipeline(options);
    case BlendMode::kDifference:
      return renderer.GetFramebufferBlendDifferencePipeline(options);
    case BlendMode::kExclusion:
      return renderer.GetFramebufferBlendExclusionPipeline(options);
    case BlendMode::kMultiply:
      return renderer.GetFramebufferBlendMultiplyPipeline(options);
    case BlendMode::kHue:
      return renderer.GetFramebufferBlendHuePipeline(options);
    case BlendMode::kSaturation:
      return renderer.GetFramebufferBlendSaturationPipeline(options);
    case BlendMode::kColor:
      return renderer.GetFramebufferBlendColorPipeline(options);
    case BlendMode::kLuminosity:
      return renderer.GetFramebufferBlendLuminosityPipeline(options);
    default:
      return {};
  }
}

}  // namespace

FramebufferBlendContents::FramebufferBlendContents() = default;

FramebufferBlendContents::~FramebufferBlendContents() = default;

const Geometry* FramebufferBlendContents::GetGeometry() const {
  return nullptr;
}

void FramebufferBlendContents::SetBlendMode(BlendMode blend_mode) {
  blend_mode_ = blend_mode;
}

void FramebufferBlendContents::SetChildContents(
    std::shared_ptr<Contents> child_contents) {
  child_contents_ = std::move(child_contents);
}

// |Contents|
std::optional<Rect> FramebufferBlendContents::GetCoverage(
    const Entity& entity) const {
  return child_contents_->GetCoverage(entity);
}

bool FramebufferBlendContents::Render(const ContentContext& renderer,
                                      const Entity& entity,
                                      RenderPass& pass) const {
  if (!renderer.GetDeviceCapabilities().SupportsFramebufferFetch()) {
    return false;
  }

  auto src_snapshot = child_contents_->RenderToSnapshot(
      renderer, entity,
      {.coverage_limit = Rect::MakeSize(pass.GetRenderTargetSize()),
       .sampler_descriptor = std::nullopt,
       .msaa_enabled = true,
       .mip_count = 1,
       .label = "FramebufferBlendContents Snapshot"});

  if (!src_snapshot.has_value()) {
    return true;
  }

  auto pipeline = GetAdvancedBlendPipeline(
      renderer, GetFramebufferBlendOptions(pass), blend_mode_);
  return RenderSnapshot(renderer, entity, pass, *src_snapshot, pipeline,
                        std::nullopt);
}

bool FramebufferBlendContents::RenderCoverageBlend(
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass,
    const Snapshot& mask,
    Color source_color,
    CoverageBlendProgram program) {
  auto options = GetFramebufferBlendOptions(pass);
  options.blend_mode = entity.GetBlendMode();
  auto pipeline =
      renderer.GetFramebufferCoverageBlendPipeline(program, options);
  return RenderSnapshot(renderer, entity, pass, mask, pipeline, source_color);
}

bool FramebufferBlendContents::RenderSnapshot(
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass,
    const Snapshot& src_snapshot,
    PipelineRef pipeline,
    std::optional<Color> coverage_source_color) {
  if (!pipeline) {
    return false;
  }
  using VS = FramebufferBlendPipelineHandle::VertexShader;
  using FS = FramebufferBlendPipelineHandle::FragmentShader;
  auto& data_host_buffer = renderer.GetTransientsDataBuffer();
  auto size = src_snapshot.texture->GetSize();

  std::array<VS::PerVertexData, 4> vertices = {
      VS::PerVertexData{Point(0, 0), Point(0, 0)},
      VS::PerVertexData{Point(size.width, 0), Point(1, 0)},
      VS::PerVertexData{Point(0, size.height), Point(0, 1)},
      VS::PerVertexData{Point(size.width, size.height), Point(1, 1)},
  };

  pass.SetCommandLabel("Framebuffer Advanced Blend Filter");
  pass.SetVertexBuffer(
      CreateVertexBuffer(vertices, renderer.GetTransientsDataBuffer()));

  pass.SetPipeline(pipeline);

  VS::FrameInfo frame_info;
  FS::FragInfo frag_info{};

  auto src_sampler_descriptor = src_snapshot.sampler_descriptor;
  if (renderer.GetDeviceCapabilities().SupportsDecalSamplerAddressMode()) {
    src_sampler_descriptor.width_address_mode = SamplerAddressMode::kDecal;
    src_sampler_descriptor.height_address_mode = SamplerAddressMode::kDecal;
  }
  raw_ptr<const Sampler> src_sampler =
      renderer.GetContext()->GetSamplerLibrary()->GetSampler(
          src_sampler_descriptor);
  FS::BindTextureSamplerSrc(pass, src_snapshot.texture, src_sampler);

  frame_info.mvp = Entity::GetShaderTransform(entity.GetShaderClipDepth(), pass,
                                              src_snapshot.transform);
  VS::BindFrameInfo(pass, data_host_buffer.EmplaceUniform(frame_info));

  frag_info.src_input_alpha = src_snapshot.opacity;
  frag_info.dst_input_alpha = 1.0;
  if (coverage_source_color) {
    frag_info.coverage_source_color =
        ToVector(coverage_source_color->Premultiply());
  }
  FS::BindFragInfo(pass, data_host_buffer.EmplaceUniform(frag_info));

  return pass.Draw().ok();
}

}  // namespace impeller
