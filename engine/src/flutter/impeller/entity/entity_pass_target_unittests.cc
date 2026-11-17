// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "flutter/testing/testing.h"
#include "gtest/gtest.h"
#include "impeller/core/formats.h"
#include "impeller/entity/entity_pass_target.h"
#include "impeller/entity/entity_playground.h"

namespace impeller {
namespace testing {

using EntityPassTargetTest = EntityPlayground;
INSTANTIATE_PLAYGROUND_SUITE(EntityPassTargetTest);

TEST_P(EntityPassTargetTest, SwapWithMSAATexture) {
  if (GetContentContext()
          .GetDeviceCapabilities()
          .SupportsImplicitResolvingMSAA()) {
    GTEST_SKIP() << "Implicit MSAA is used on this device.";
  }
  ContentContext& content_context = GetContentContext();
  auto buffer = content_context.GetContext()->CreateCommandBuffer();
  auto render_target =
      GetContentContext().GetRenderTargetCache()->CreateOffscreenMSAA(
          *content_context.GetContext(), {100, 100},
          /*mip_count=*/1);

  auto entity_pass_target = EntityPassTarget(render_target, false, false);

  auto color0 = entity_pass_target.GetRenderTarget().GetColorAttachment(0);
  auto msaa_tex = color0.texture;
  auto resolve_tex = color0.resolve_texture;

  entity_pass_target.Flip(content_context);

  color0 = entity_pass_target.GetRenderTarget().GetColorAttachment(0);

  ASSERT_EQ(msaa_tex, color0.texture);
  ASSERT_NE(resolve_tex, color0.resolve_texture);
}

TEST_P(EntityPassTargetTest, KeepsResolveWithItsMSAAAttachmentsAcrossFrames) {
  if (GetBackend() != PlaygroundBackend::kVulkan ||
      !GetContentContext().GetDeviceCapabilities().SupportsOffscreenMSAA()) {
    GTEST_SKIP() << "This test needs explicit MSAA resolves on Vulkan.";
  }

  auto& content_context = GetContentContext();
  const auto& context = *content_context.GetContext();
  const auto& cache = content_context.GetRenderTargetCache();
  const auto default_format =
      content_context.GetDeviceCapabilities().GetDefaultColorFormat();
  const auto format = default_format == PixelFormat::kR8G8B8A8UNormInt
                          ? PixelFormat::kB8G8R8A8UNormInt
                          : PixelFormat::kR8G8B8A8UNormInt;
  const auto create_target = [&](PixelFormat target_format) {
    return cache->CreateOffscreenMSAA(
        context, {100, 100}, /*mip_count=*/1, "MSAA render target",
        RenderTarget::kDefaultColorAttachmentConfigMSAA,
        RenderTarget::kDefaultStencilAttachmentConfig, nullptr, nullptr,
        nullptr, target_format);
  };

  cache->Start();
  auto target = create_target(format);
  ASSERT_TRUE(target.IsValid());
  EntityPassTarget pass(target, false, false);
  const auto primary = pass.Flip(content_context);
  const auto secondary = pass.GetRenderTarget().GetRenderTargetTexture();
  ASSERT_TRUE(primary);
  ASSERT_TRUE(secondary);
  ASSERT_NE(primary, secondary);
  EXPECT_EQ(secondary->GetTextureDescriptor(), primary->GetTextureDescriptor());

  // Once the caller drops a resolve, later draws in the same frame may still
  // need its contents. Do not give the cached texture out again in that frame.
  ASSERT_EQ(pass.Flip(content_context), secondary);
  pass.RemoveSecondary();
  ASSERT_EQ(pass.Flip(content_context), primary);
  EXPECT_NE(pass.GetRenderTarget().GetRenderTargetTexture(), secondary);
  cache->End();

  cache->Start();
  auto other_format = create_target(default_format);
  ASSERT_TRUE(other_format.IsValid());
  EXPECT_EQ(other_format.GetRenderTargetPixelFormat(), default_format);
  auto next_target = create_target(format);
  ASSERT_TRUE(next_target.IsValid());
  EXPECT_EQ(next_target.GetRenderTargetTexture(), primary);
  EXPECT_EQ(next_target.GetColorAttachment(0).texture,
            target.GetColorAttachment(0).texture);
  EntityPassTarget next_pass(next_target, false, false);
  ASSERT_EQ(next_pass.Flip(content_context), primary);
  EXPECT_EQ(next_pass.GetRenderTarget().GetRenderTargetTexture(), secondary);
  auto other_target = create_target(format);
  ASSERT_TRUE(other_target.IsValid());
  EXPECT_NE(other_target.GetRenderTargetTexture(), secondary);
  cache->End();
}

TEST_P(EntityPassTargetTest, SwapWithMSAAImplicitResolve) {
  ContentContext& content_context = GetContentContext();
  auto buffer = content_context.GetContext()->CreateCommandBuffer();
  auto context = content_context.GetContext();
  auto& allocator = *context->GetResourceAllocator();

  // Emulate implicit MSAA resolve by making color resolve and msaa texture the
  // same.
  RenderTarget render_target;
  {
    PixelFormat pixel_format =
        context->GetCapabilities()->GetDefaultColorFormat();

    // Create MSAA color texture.

    TextureDescriptor color0_tex_desc;
    color0_tex_desc.storage_mode = StorageMode::kDevicePrivate;
    color0_tex_desc.type = TextureType::kTexture2DMultisample;
    color0_tex_desc.sample_count = SampleCount::kCount4;
    color0_tex_desc.format = pixel_format;
    color0_tex_desc.size = ISize{100, 100};
    color0_tex_desc.usage = TextureUsage::kRenderTarget;

    auto color0_msaa_tex = allocator.CreateTexture(color0_tex_desc);

    // Color attachment.

    ColorAttachment color0;
    color0.load_action = LoadAction::kDontCare;
    color0.store_action = StoreAction::kStoreAndMultisampleResolve;
    color0.texture = color0_msaa_tex;
    color0.resolve_texture = color0_msaa_tex;

    render_target.SetColorAttachment(color0, 0u);
    render_target.SetStencilAttachment(std::nullopt);
  }

  auto entity_pass_target = EntityPassTarget(render_target, false, true);

  auto color0 = entity_pass_target.GetRenderTarget().GetColorAttachment(0);
  auto msaa_tex = color0.texture;
  auto resolve_tex = color0.resolve_texture;

  ASSERT_EQ(msaa_tex, resolve_tex);

  entity_pass_target.Flip(content_context);

  color0 = entity_pass_target.GetRenderTarget().GetColorAttachment(0);

  ASSERT_NE(msaa_tex, color0.texture);
  ASSERT_NE(resolve_tex, color0.resolve_texture);
  ASSERT_EQ(color0.texture, color0.resolve_texture);
}

}  // namespace testing
}  // namespace impeller
