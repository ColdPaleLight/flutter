// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "framebuffer_blend_contents.h"

#include "impeller/entity/contents/content_context.h"
#include "impeller/renderer/render_pass.h"

namespace impeller {

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

static void SetSnapshotGeometry(const ContentContext& renderer,
                                RenderPass& pass,
                                const Snapshot& snapshot) {
  using VS = FramebufferBlendPipelineHandle::VertexShader;
  auto size = snapshot.texture->GetSize();

  std::array<VS::PerVertexData, 4> vertices = {
      VS::PerVertexData{Point(0, 0), Point(0, 0)},
      VS::PerVertexData{Point(size.width, 0), Point(1, 0)},
      VS::PerVertexData{Point(0, size.height), Point(0, 1)},
      VS::PerVertexData{Point(size.width, size.height), Point(1, 1)},
  };

  pass.SetCommandLabel("Framebuffer Advanced Blend Filter");
  pass.SetVertexBuffer(
      CreateVertexBuffer(vertices, renderer.GetTransientsDataBuffer()));
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

  auto options = OptionsFromPass(pass);
  options.blend_state = {};
  options.primitive_type = PrimitiveType::kTriangleStrip;

  SetSnapshotGeometry(renderer, pass, *src_snapshot);

  switch (blend_mode_) {
    case BlendMode::kScreen:
      pass.SetPipeline(renderer.GetFramebufferBlendScreenPipeline(options));
      break;
    case BlendMode::kOverlay:
      pass.SetPipeline(renderer.GetFramebufferBlendOverlayPipeline(options));
      break;
    case BlendMode::kDarken:
      pass.SetPipeline(renderer.GetFramebufferBlendDarkenPipeline(options));
      break;
    case BlendMode::kLighten:
      pass.SetPipeline(renderer.GetFramebufferBlendLightenPipeline(options));
      break;
    case BlendMode::kColorDodge:
      pass.SetPipeline(renderer.GetFramebufferBlendColorDodgePipeline(options));
      break;
    case BlendMode::kColorBurn:
      pass.SetPipeline(renderer.GetFramebufferBlendColorBurnPipeline(options));
      break;
    case BlendMode::kHardLight:
      pass.SetPipeline(renderer.GetFramebufferBlendHardLightPipeline(options));
      break;
    case BlendMode::kSoftLight:
      pass.SetPipeline(renderer.GetFramebufferBlendSoftLightPipeline(options));
      break;
    case BlendMode::kDifference:
      pass.SetPipeline(renderer.GetFramebufferBlendDifferencePipeline(options));
      break;
    case BlendMode::kExclusion:
      pass.SetPipeline(renderer.GetFramebufferBlendExclusionPipeline(options));
      break;
    case BlendMode::kMultiply:
      pass.SetPipeline(renderer.GetFramebufferBlendMultiplyPipeline(options));
      break;
    case BlendMode::kHue:
      pass.SetPipeline(renderer.GetFramebufferBlendHuePipeline(options));
      break;
    case BlendMode::kSaturation:
      pass.SetPipeline(renderer.GetFramebufferBlendSaturationPipeline(options));
      break;
    case BlendMode::kColor:
      pass.SetPipeline(renderer.GetFramebufferBlendColorPipeline(options));
      break;
    case BlendMode::kLuminosity:
      pass.SetPipeline(renderer.GetFramebufferBlendLuminosityPipeline(options));
      break;
    default:
      return false;
  }

  return DrawSnapshot(renderer, entity, pass, *src_snapshot);
}

bool FramebufferBlendContents::RenderCoverageBlend(
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass,
    const Snapshot& mask,
    Color source_color,
    CoverageBlendProgram program) {
  auto options = OptionsFromPass(pass);
  options.blend_state = {};
  options.primitive_type = PrimitiveType::kTriangleStrip;
  auto pipeline =
      renderer.GetFramebufferCoverageBlendPipeline(program, options);
  if (!pipeline) {
    return false;
  }

  SetSnapshotGeometry(renderer, pass, mask);
  pass.SetPipeline(pipeline);
  return DrawSnapshot(renderer, entity, pass, mask, source_color);
}

bool FramebufferBlendContents::DrawSnapshot(
    const ContentContext& renderer,
    const Entity& entity,
    RenderPass& pass,
    const Snapshot& src_snapshot,
    std::optional<Color> coverage_source_color) {
  using VS = FramebufferBlendPipelineHandle::VertexShader;
  using FS = FramebufferBlendPipelineHandle::FragmentShader;

  auto& data_host_buffer = renderer.GetTransientsDataBuffer();

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
