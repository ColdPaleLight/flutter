// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/entity/render_target_cache.h"
#include "impeller/core/formats.h"
#include "impeller/renderer/context.h"
#include "impeller/renderer/render_target.h"

namespace impeller {

RenderTargetCache::RenderTargetCache(std::shared_ptr<Allocator> allocator,
                                     uint32_t keep_alive_frame_count)
    : RenderTargetAllocator(std::move(allocator)),
      keep_alive_frame_count_(keep_alive_frame_count) {}

void RenderTargetCache::Start() {
  cache_disabled_count_ = 0;
  for (auto& td : render_target_data_) {
    td.used_this_frame = false;
    td.secondary_resolve_used_this_frame = false;
  }
}

void RenderTargetCache::End() {
  cache_disabled_count_ = 0;
  std::vector<RenderTargetData> retain;

  for (RenderTargetData& td : render_target_data_) {
    if (td.secondary_resolve_texture && !td.secondary_resolve_used_this_frame) {
      if (td.secondary_resolve_keep_alive_frame_count > 0) {
        td.secondary_resolve_keep_alive_frame_count--;
      } else {
        td.secondary_resolve_texture.reset();
      }
    }
    if (td.used_this_frame) {
      retain.push_back(td);
    } else if (td.keep_alive_frame_count > 0) {
      td.keep_alive_frame_count--;
      retain.push_back(td);
    }
  }
  render_target_data_.swap(retain);
}

void RenderTargetCache::DisableCache() {
  cache_disabled_count_++;
}

bool RenderTargetCache::CacheEnabled() const {
  return cache_disabled_count_ == 0;
}

void RenderTargetCache::EnableCache() {
  FML_DCHECK(cache_disabled_count_ > 0);
  if (cache_disabled_count_ == 0) {
    return;
  }
  cache_disabled_count_--;
}

void RenderTargetCache::Clear() {
  render_target_data_.clear();
}

RenderTarget RenderTargetCache::CreateOffscreen(
    const Context& context,
    ISize size,
    int mip_count,
    std::string_view label,
    RenderTarget::AttachmentConfig color_attachment_config,
    std::optional<RenderTarget::AttachmentConfig> stencil_attachment_config,
    const std::shared_ptr<Texture>& existing_color_texture,
    const std::shared_ptr<Texture>& existing_depth_stencil_texture,
    std::optional<PixelFormat> target_pixel_format) {
  if (size.IsEmpty()) {
    return {};
  }

  FML_DCHECK(existing_color_texture == nullptr &&
             existing_depth_stencil_texture == nullptr);
  auto config = RenderTargetConfig{
      .size = size,
      .mip_count = static_cast<size_t>(mip_count),
      .has_msaa = false,
      .has_depth_stencil = stencil_attachment_config.has_value(),
      .color_format = target_pixel_format.value_or(
          context.GetCapabilities()->GetDefaultColorFormat()),
  };

  if (CacheEnabled()) {
    for (RenderTargetData& render_target_data : render_target_data_) {
      const RenderTargetConfig other_config = render_target_data.config;
      if (!render_target_data.used_this_frame && other_config == config) {
        render_target_data.used_this_frame = true;
        render_target_data.keep_alive_frame_count = keep_alive_frame_count_;
        ColorAttachment color0 =
            render_target_data.render_target.GetColorAttachment(0);
        std::optional<DepthAttachment> depth =
            render_target_data.render_target.GetDepthAttachment();
        std::shared_ptr<Texture> depth_tex = depth ? depth->texture : nullptr;
        return RenderTargetAllocator::CreateOffscreen(
            context, size, mip_count, label, color_attachment_config,
            stencil_attachment_config, color0.texture, depth_tex,
            target_pixel_format);
      }
    }
  }
  RenderTarget created_target = RenderTargetAllocator::CreateOffscreen(
      context, size, mip_count, label, color_attachment_config,
      stencil_attachment_config, nullptr, nullptr, target_pixel_format);
  if (!created_target.IsValid()) {
    return created_target;
  }
  if (CacheEnabled()) {
    render_target_data_.push_back(RenderTargetData{
        .used_this_frame = true,                            //
        .keep_alive_frame_count = keep_alive_frame_count_,  //
        .config = config,                                   //
        .render_target = created_target                     //
    });
  }
  return created_target;
}

RenderTarget RenderTargetCache::CreateOffscreenMSAA(
    const Context& context,
    ISize size,
    int mip_count,
    std::string_view label,
    RenderTarget::AttachmentConfigMSAA color_attachment_config,
    std::optional<RenderTarget::AttachmentConfig> stencil_attachment_config,
    const std::shared_ptr<Texture>& existing_color_msaa_texture,
    const std::shared_ptr<Texture>& existing_color_resolve_texture,
    const std::shared_ptr<Texture>& existing_depth_stencil_texture,
    std::optional<PixelFormat> target_pixel_format) {
  if (size.IsEmpty()) {
    return {};
  }

  FML_DCHECK(existing_color_msaa_texture == nullptr &&
             existing_color_resolve_texture == nullptr &&
             existing_depth_stencil_texture == nullptr);
  auto config = RenderTargetConfig{
      .size = size,
      .mip_count = static_cast<size_t>(mip_count),
      .has_msaa = true,
      .has_depth_stencil = stencil_attachment_config.has_value(),
      .color_format = target_pixel_format.value_or(
          context.GetCapabilities()->GetDefaultColorFormat()),
  };
  if (CacheEnabled()) {
    for (RenderTargetData& render_target_data : render_target_data_) {
      const RenderTargetConfig other_config = render_target_data.config;
      if (!render_target_data.used_this_frame && other_config == config) {
        render_target_data.used_this_frame = true;
        render_target_data.keep_alive_frame_count = keep_alive_frame_count_;
        ColorAttachment color0 =
            render_target_data.render_target.GetColorAttachment(0);
        std::optional<DepthAttachment> depth =
            render_target_data.render_target.GetDepthAttachment();
        std::shared_ptr<Texture> depth_tex = depth ? depth->texture : nullptr;
        return RenderTargetAllocator::CreateOffscreenMSAA(
            context, size, mip_count, label, color_attachment_config,
            stencil_attachment_config, color0.texture, color0.resolve_texture,
            depth_tex, target_pixel_format);
      }
    }
  }
  RenderTarget created_target = RenderTargetAllocator::CreateOffscreenMSAA(
      context, size, mip_count, label, color_attachment_config,
      stencil_attachment_config, nullptr, nullptr, nullptr,
      target_pixel_format);
  if (!created_target.IsValid()) {
    return created_target;
  }
  if (CacheEnabled()) {
    render_target_data_.push_back(RenderTargetData{
        .used_this_frame = true,                            //
        .keep_alive_frame_count = keep_alive_frame_count_,  //
        .config = config,                                   //
        .render_target = created_target                     //
    });
  }
  return created_target;
}

static bool HasSameView(const Attachment& a, const Attachment& b) {
  return a.texture == b.texture && a.mip_level == b.mip_level &&
         a.slice == b.slice;
}

template <class T>
static bool HasSameView(const std::optional<T>& a, const std::optional<T>& b) {
  return a.has_value() == b.has_value() && (!a || HasSameView(*a, *b));
}

std::shared_ptr<Texture> RenderTargetCache::CreateSecondaryResolveTexture(
    const RenderTarget& target) {
  if (CacheEnabled() && target.GetMaxColorAttachmentBindIndex() == 0) {
    const auto& color = target.GetColorAttachment(0);
    for (auto& data : render_target_data_) {
      if (!data.config.has_msaa || !data.used_this_frame ||
          data.secondary_resolve_used_this_frame) {
        continue;
      }
      // Vulkan caches the framebuffer on the resolve texture. Reuse the
      // alternate only with the original target's attachment views.
      const auto& cached = data.render_target;
      const auto& cached_color = cached.GetColorAttachment(0);
      if (!HasSameView(cached_color, color) ||
          cached_color.resolve_texture != color.resolve_texture ||
          !HasSameView(cached.GetDepthAttachment(),
                       target.GetDepthAttachment()) ||
          !HasSameView(cached.GetStencilAttachment(),
                       target.GetStencilAttachment())) {
        continue;
      }
      if (!data.secondary_resolve_texture) {
        data.secondary_resolve_texture =
            RenderTargetAllocator::CreateSecondaryResolveTexture(target);
      }
      data.secondary_resolve_used_this_frame =
          data.secondary_resolve_texture != nullptr;
      data.secondary_resolve_keep_alive_frame_count = keep_alive_frame_count_;
      return data.secondary_resolve_texture;
    }
  }
  return RenderTargetAllocator::CreateSecondaryResolveTexture(target);
}

size_t RenderTargetCache::CachedTextureCount() const {
  return render_target_data_.size();
}

}  // namespace impeller
