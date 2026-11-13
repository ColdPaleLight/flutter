// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_IMPELLER_ENTITY_CONTENTS_CIRCLE_CONTENTS_H_
#define FLUTTER_IMPELLER_ENTITY_CONTENTS_CIRCLE_CONTENTS_H_

#include <memory>

#include "flutter/impeller/entity/contents/color_source_contents.h"
#include "flutter/impeller/entity/contents/contents.h"
#include "impeller/entity/geometry/circle_geometry.h"

namespace impeller {
struct BlendBackdrop;

class CircleContents : public ColorSourceContents {
 public:
  static std::unique_ptr<CircleContents>
  Make(std::unique_ptr<CircleGeometry> geometry, Color color, bool stroked);

  bool Render(const ContentContext& renderer,
              const Entity& entity,
              RenderPass& pass) const override;

  std::optional<BlendSourceAlpha> GetCoverageSourceAlpha() const override;

  bool RenderWithCoverageBlend(const ContentContext& renderer,
                               const Entity& entity,
                               RenderPass& pass,
                               const CoverageBlend& blend) const override;

  std::optional<Rect> GetCoverage(const Entity& entity) const override;

  const Geometry* GetGeometry() const override;

 private:
  bool RenderCircle(const ContentContext& renderer,
                    const Entity& entity,
                    RenderPass& pass,
                    const BlendPlan* plan,
                    const BlendBackdrop* backdrop) const;

  explicit CircleContents(std::unique_ptr<CircleGeometry> geometry,
                          Color color,
                          bool stroked,
                          Scalar aa_padding);

  std::unique_ptr<CircleGeometry> geometry_;
  Color color_;
  bool stroked_;
  Scalar aa_padding_;
};
}  // namespace impeller

#endif  // FLUTTER_IMPELLER_ENTITY_CONTENTS_CIRCLE_CONTENTS_H_
