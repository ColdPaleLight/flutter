// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/display_list/dl_golden_unittests.h"

#include <array>
#include <memory>
#include <utility>

#include "display_list/dl_color.h"
#include "display_list/dl_paint.h"
#include "display_list/geometry/dl_geometry_types.h"
#include "display_list/geometry/dl_path_builder.h"
#include "flutter/display_list/dl_builder.h"
#include "flutter/fml/closure.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/impeller/display_list/testing/render_text_in_canvas.h"
#include "flutter/impeller/display_list/testing/rmse.h"
#include "flutter/testing/testing.h"
#include "gtest/gtest.h"
#include "impeller/display_list/dl_dispatcher.h"
#include "impeller/display_list/dl_text_impeller.h"
#include "impeller/display_list/skia_conversions.h"
#include "impeller/renderer/blit_pass.h"
#include "impeller/renderer/command_queue.h"
#include "impeller/renderer/testing/mocks.h"
#include "impeller/typographer/backends/skia/typographer_context_skia.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "txt/platform.h"

#if IMPELLER_ENABLE_VULKAN
#include "impeller/renderer/backend/vulkan/capabilities_vk.h"
#include "impeller/renderer/backend/vulkan/context_vk.h"
#include "impeller/renderer/backend/vulkan/surface_context_vk.h"
#endif

namespace flutter {
namespace testing {

using impeller::Degrees;
using impeller::PlaygroundBackend;
using impeller::PlaygroundTest;
using impeller::Point;
using impeller::Radians;
using impeller::Scalar;

INSTANTIATE_PLAYGROUND_SUITE(DlGoldenTest);

TEST_P(DlGoldenTest, CanDrawPaint) {
  auto draw = [](DlCanvas* canvas,
                 const std::vector<std::unique_ptr<DlImage>>& images) {
    canvas->Scale(0.2, 0.2);
    DlPaint paint;
    paint.setColor(DlColor::kCyan());
    canvas->DrawPaint(paint);
  };

  DisplayListBuilder builder;
  draw(&builder, /*images=*/{});

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DlGoldenTest, CanRenderImage) {
  auto draw = [](DlCanvas* canvas, const std::vector<sk_sp<DlImage>>& images) {
    FML_CHECK(images.size() >= 1);
    DlPaint paint;
    paint.setColor(DlColor::kRed());
    canvas->DrawImage(images[0], DlPoint(100.0, 100.0),
                      DlImageSampling::kLinear, &paint);
  };

  DisplayListBuilder builder;
  std::vector<sk_sp<DlImage>> images;
  images.emplace_back(CreateDlImageForFixture("kalimba.jpg"));
  draw(&builder, images);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

// Asserts that subpass rendering of MatrixImageFilters works.
// https://github.com/flutter/flutter/issues/147807
TEST_P(DlGoldenTest, Bug147807) {
  Point content_scale = GetContentScale();
  auto draw = [content_scale](DlCanvas* canvas,
                              const std::vector<sk_sp<DlImage>>& images) {
    canvas->Scale(content_scale.x, content_scale.y);
    DlPaint paint;
    paint.setColor(DlColor(0xfffef7ff));
    canvas->DrawRect(DlRect::MakeLTRB(0, 0, 375, 667), paint);
    paint.setColor(DlColor(0xffff9800));
    canvas->DrawRect(DlRect::MakeLTRB(0, 0, 187.5, 333.5), paint);
    paint.setColor(DlColor(0xff9c27b0));
    canvas->DrawRect(DlRect::MakeLTRB(187.5, 0, 375, 333.5), paint);
    paint.setColor(DlColor(0xff4caf50));
    canvas->DrawRect(DlRect::MakeLTRB(0, 333.5, 187.5, 667), paint);
    paint.setColor(DlColor(0xfff44336));
    canvas->DrawRect(DlRect::MakeLTRB(187.5, 333.5, 375, 667), paint);

    canvas->Save();
    {
      canvas->ClipRoundRect(
          DlRoundRect::MakeOval(DlRect::MakeLTRB(201.25, 10, 361.25, 170)),
          DlClipOp::kIntersect, true);
      DlRect save_layer_bounds = DlRect::MakeLTRB(201.25, 10, 361.25, 170);
      auto backdrop =
          DlImageFilter::MakeMatrix(DlMatrix::MakeRow(3, 0, 0.0, -280,  //
                                                      0, 3, 0.0, -920,  //
                                                      0, 0, 1.0, 0.0,   //
                                                      0, 0, 0.0, 1.0),
                                    DlImageSampling::kLinear);
      canvas->SaveLayer(save_layer_bounds, /*paint=*/nullptr, backdrop.get());
      {
        canvas->Translate(201.25, 10);
        auto paint = DlPaint()
                         .setAntiAlias(true)
                         .setColor(DlColor(0xff2196f3))
                         .setStrokeWidth(5)
                         .setDrawStyle(DlDrawStyle::kStroke);
        canvas->DrawCircle(DlPoint(80, 80), 80, paint);
      }
      canvas->Restore();
    }
    canvas->Restore();
  };

  DisplayListBuilder builder;
  std::vector<sk_sp<DlImage>> images;
  draw(&builder, images);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DlGoldenTest, FractionalDilation) {
  auto draw = [](DlCanvas* canvas) {
    const DlRect rect = DlRect::MakeLTRB(32, 32, 382, 382);
    DlPaint layer_paint;
    layer_paint.setImageFilter(DlImageFilter::MakeDilate(3.95f, 3.95f));
    canvas->SaveLayer(rect.Expand(8.0f), &layer_paint);
    canvas->DrawRect(rect, DlPaint().setColor(DlColor::kRed()));
    canvas->Restore();
    canvas->DrawRect(rect, DlPaint().setColor(DlColor::kBlack()));
  };

  DisplayListBuilder builder;
  builder.Scale(GetContentScale().x, GetContentScale().y);
  builder.DrawColor(DlColor::kWhite(), DlBlendMode::kSrc);

  draw(&builder);

  builder.Translate(440, 0);
  builder.Scale(1.3f, 1.3f);
  draw(&builder);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

namespace {
void DrawBlurGrid(DlCanvas* canvas) {
  DlPaint paint;
  paint.setColor(DlColor(0xfffef7ff));
  Scalar width = 150;
  Scalar height = 150;
  Scalar gap = 80;
  std::vector<Scalar> blur_radii = {10, 30, 50};
  for (size_t i = 0; i < blur_radii.size(); ++i) {
    Scalar blur_radius = blur_radii[i];
    auto blur_filter = std::make_shared<flutter::DlBlurMaskFilter>(
        flutter::DlBlurStyle::kNormal, blur_radius);
    paint.setMaskFilter(blur_filter);
    Scalar yval = gap + i * (gap + height);
    canvas->DrawRoundRect(
        DlRoundRect::MakeNinePatch(DlRect::MakeXYWH(gap, yval, width, height),
                                   10, 10, 10, 10),
        paint);
    canvas->DrawRoundRect(
        DlRoundRect::MakeNinePatch(
            DlRect::MakeXYWH(2.0 * gap + width, yval, width, height),  //
            9, 10, 10, 10),
        paint);
  }
}
}  // namespace

TEST_P(DlGoldenTest, GaussianVsRRectBlur) {
  Point content_scale = GetContentScale();
  auto draw = [content_scale](DlCanvas* canvas,
                              const std::vector<sk_sp<DlImage>>& images) {
    canvas->Scale(content_scale.x, content_scale.y);
    canvas->DrawPaint(DlPaint().setColor(DlColor(0xff112233)));
    DrawBlurGrid(canvas);
  };

  DisplayListBuilder builder;
  std::vector<sk_sp<DlImage>> images;
  draw(&builder, images);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DlGoldenTest, GaussianVsRRectBlurScaled) {
  Point content_scale = GetContentScale();
  auto draw = [content_scale](DlCanvas* canvas,
                              const std::vector<sk_sp<DlImage>>& images) {
    canvas->Scale(content_scale.x, content_scale.y);
    canvas->DrawPaint(DlPaint().setColor(DlColor(0xff112233)));
    canvas->Scale(0.33, 0.33);
    DrawBlurGrid(canvas);
  };

  DisplayListBuilder builder;
  std::vector<sk_sp<DlImage>> images;
  draw(&builder, images);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DlGoldenTest, GaussianVsRRectBlurScaledRotated) {
  Point content_scale = GetContentScale();
  auto draw = [content_scale](DlCanvas* canvas,
                              const std::vector<sk_sp<DlImage>>& images) {
    canvas->Scale(content_scale.x, content_scale.y);
    canvas->Translate(200, 200);
    canvas->DrawPaint(DlPaint().setColor(DlColor(0xff112233)));
    canvas->Scale(0.33, 0.33);
    canvas->Translate(300, 300);
    canvas->Rotate(45);
    canvas->Translate(-300, -300);
    DrawBlurGrid(canvas);
  };

  DisplayListBuilder builder;
  std::vector<sk_sp<DlImage>> images;
  draw(&builder, images);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DlGoldenTest, FastVsGeneralGaussianMaskBlur) {
  DisplayListBuilder builder;
  builder.Scale(GetContentScale().x, GetContentScale().y);
  builder.DrawColor(DlColor::kWhite(), DlBlendMode::kSrc);

  auto blur_sigmas = std::array{5.0f, 10.0f, 20.0f};
  auto blur_colors = std::array{
      DlColor::kBlue(),
      DlColor::kGreen(),
      DlColor::kMaroon(),
  };

  auto make_rrect_path = [](const DlRect& rect, DlScalar rx,
                            DlScalar ry) -> DlPath {
    auto add_corner = [](DlPathBuilder& path_builder, DlPoint corner,
                         DlVector2 relative_from, DlVector2 relative_to,
                         bool first) {
      static const auto magic = DlPathBuilder::kArcApproximationMagic;

      if (first) {
        path_builder.MoveTo(corner + relative_from);
      } else {
        path_builder.LineTo(corner + relative_from);
      }
      // These fractions should be (1 - magic) to make a proper rrect
      // path, but historically these equations were as written here.
      // On the plus side, they ensure that we will not optimize this
      // path as "Hey, look, it's an RRect", but the DrawPath gaussians
      // will otherwise not be identical to the versions drawn with
      // DrawRoundRect
      path_builder.CubicCurveTo(corner + relative_from * magic,
                                corner + relative_to * magic,
                                corner + relative_to);
    };

    DlPathBuilder path_builder;
    add_corner(path_builder, rect.GetRightTop(),  //
               DlVector2(-rx, 0.0f), DlVector2(0.0f, ry), true);
    add_corner(path_builder, rect.GetRightBottom(),  //
               DlVector2(0.0f, -ry), DlVector2(-rx, 0.0f), false);
    add_corner(path_builder, rect.GetLeftBottom(),  //
               DlVector2(rx, 0.0f), DlVector2(0.0f, -ry), false);
    add_corner(path_builder, rect.GetLeftTop(),  //
               DlVector2(0.0f, ry), DlVector2(rx, 0.0f), false);
    return path_builder.TakePath();
  };

  for (size_t i = 0; i < blur_sigmas.size(); i++) {
    auto rect = DlRect::MakeXYWH(i * 320.0f + 50.0f, 50.0f, 100.0f, 100.0f);
    DlPaint paint = DlPaint()  //
                        .setColor(blur_colors[i])
                        .setMaskFilter(DlBlurMaskFilter::Make(
                            DlBlurStyle::kNormal, blur_sigmas[i]));

    builder.DrawRoundRect(DlRoundRect::MakeRectXY(rect, 10.0f, 10.0f), paint);
    rect = rect.Shift(150.0f, 0.0f);
    builder.DrawPath(make_rrect_path(rect, 10.0f, 10.0f), paint);
    rect = rect.Shift(-150.0f, 0.0f);

    rect = rect.Shift(0.0f, 200.0f);
    builder.DrawRoundRect(DlRoundRect::MakeRectXY(rect, 10.0f, 30.0f), paint);
    rect = rect.Shift(150.0f, 0.0f);
    builder.DrawPath(make_rrect_path(rect, 10.0f, 20.0f), paint);
    rect = rect.Shift(-150.0f, 0.0f);

    rect = rect.Shift(0.0f, 200.0f);
    builder.DrawRoundRect(DlRoundRect::MakeRectXY(rect, 30.0f, 10.0f), paint);
    rect = rect.Shift(150.0f, 0.0f);
    builder.DrawPath(make_rrect_path(rect, 20.0f, 10.0f), paint);
    rect = rect.Shift(-150.0f, 0.0f);
  }

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DlGoldenTest, DashedLinesTest) {
  Point content_scale = GetContentScale();
  auto draw = [content_scale](DlCanvas* canvas,
                              const std::vector<sk_sp<DlImage>>& images) {
    canvas->Scale(content_scale.x, content_scale.y);
    canvas->DrawPaint(DlPaint().setColor(DlColor::kWhite()));

    auto draw_one = [canvas](DlStrokeCap cap, Scalar x, Scalar y,
                             Scalar dash_on, Scalar dash_off) {
      Point center = Point(x, y);
      Scalar inner = 20.0f;
      Scalar outer = 100.0f;
      DlPaint thick_paint = DlPaint()
                                .setColor(DlColor::kBlue())
                                .setStrokeCap(cap)
                                .setStrokeWidth(8.0f);
      DlPaint middle_paint = DlPaint()
                                 .setColor(DlColor::kGreen())
                                 .setStrokeCap(cap)
                                 .setStrokeWidth(5.0f);
      DlPaint thin_paint = DlPaint()
                               .setColor(DlColor::kMagenta())
                               .setStrokeCap(cap)
                               .setStrokeWidth(2.0f);
      for (int degrees = 0; degrees < 360; degrees += 30) {
        Point delta = Point(1.0f, 0.0f).Rotate(Degrees(degrees));
        canvas->DrawDashedLine(center + inner * delta, center + outer * delta,
                               dash_on, dash_off, thick_paint);
        canvas->DrawDashedLine(center + inner * delta, center + outer * delta,
                               dash_on, dash_off, middle_paint);
        canvas->DrawDashedLine(center + inner * delta, center + outer * delta,
                               dash_on, dash_off, thin_paint);
      }
    };

    draw_one(DlStrokeCap::kButt, 150.0f, 150.0f, 15.0f, 10.0f);
    draw_one(DlStrokeCap::kSquare, 400.0f, 150.0f, 15.0f, 10.0f);
    draw_one(DlStrokeCap::kRound, 150.0f, 400.0f, 15.0f, 10.0f);
    draw_one(DlStrokeCap::kRound, 400.0f, 400.0f, 0.0f, 11.0f);

    // Make sure the rendering op responds appropriately to clipping
    canvas->Save();
    DlPathBuilder path_builder;
    path_builder.MoveTo(DlPoint(275.0f, 225.0f));
    path_builder.LineTo(DlPoint(325.0f, 275.0f));
    path_builder.LineTo(DlPoint(275.0f, 325.0f));
    path_builder.LineTo(DlPoint(225.0f, 275.0f));
    canvas->ClipPath(path_builder.TakePath());
    canvas->DrawColor(DlColor::kYellow());
    draw_one(DlStrokeCap::kRound, 275.0f, 275.0f, 15.0f, 10.0f);
    canvas->Restore();
  };

  DisplayListBuilder builder;
  std::vector<sk_sp<DlImage>> images;
  draw(&builder, images);

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

TEST_P(DlGoldenTest, SaveLayerAtFractionalValue) {
  // Draws a stroked rounded rect at a fractional pixel value. The coverage must
  // be adjusted so that we still have room to draw it, even though it lies on
  // the fractional bounds of the saveLayer.
  DisplayListBuilder builder;
  builder.DrawPaint(DlPaint().setColor(DlColor::kWhite()));
  auto save_paint = DlPaint().setAlpha(100);
  builder.SaveLayer(std::nullopt, &save_paint);

  builder.DrawRoundRect(DlRoundRect::MakeRectRadius(
                            DlRect::MakeLTRB(10.5, 10.5, 200.5, 200.5), 10),
                        DlPaint()
                            .setDrawStyle(DlDrawStyle::kStroke)
                            .setStrokeWidth(1.5)
                            .setColor(DlColor::kBlack()));
  builder.DrawCircle(DlPoint::MakeXY(100, 100), 50.5,
                     DlPaint().setColor(DlColor::kAqua()));
  builder.DrawCircle(DlPoint::MakeXY(110, 110), 50.5,
                     DlPaint().setColor(DlColor::kCyan()));

  builder.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(builder.Build()));
}

namespace {
int32_t CalculateMaxY(const impeller::testing::Screenshot* img) {
  const uint32_t* ptr = reinterpret_cast<const uint32_t*>(img->GetBytes());
  int32_t max_y = 0;
  for (uint32_t i = 0; i < img->GetHeight(); ++i) {
    for (uint32_t j = 0; j < img->GetWidth(); ++j) {
      uint32_t pixel = *ptr++;
      if ((pixel & 0x00ffffff) != 0) {
        max_y = std::max(max_y, static_cast<int32_t>(i));
      }
    }
  }
  return max_y;
}

std::optional<int32_t> CalculateSpaceBetweenUI(
    const impeller::testing::Screenshot* img,
    int32_t y) {
  if (y < 0 || std::cmp_greater_equal(y, img->GetHeight())) {
    return {};
  }
  const uint32_t* ptr = reinterpret_cast<const uint32_t*>(img->GetBytes());
  ptr += img->GetWidth() * y;
  std::vector<size_t> boundaries;
  uint32_t value = *ptr++;
  for (size_t i = 1; i < img->GetWidth(); ++i) {
    if (((*ptr & 0x00ffffff) != 0) != ((value & 0x00ffffff) != 0)) {
      boundaries.push_back(i);
    }
    value = *ptr++;
  }

  if (boundaries.size() != 6) {
    return {};
  }
  return boundaries[4] - boundaries[3];
}
}  // namespace

TEST_P(DlGoldenTest, BaselineHE) {
  SetWindowSize(impeller::ISize(1024, 200));
  impeller::Scalar font_size = 300;
  auto callback = [&](const char* text,
                      impeller::Scalar scale) -> sk_sp<DisplayList> {
    DisplayListBuilder builder;
    DlPaint paint;
    paint.setColor(DlColor::ARGB(1, 0, 0, 0));
    builder.DrawPaint(paint);
    builder.Scale(scale, scale);
    if (!RenderTextInCanvasSkia(&builder, text, "Roboto-Regular.ttf",
                                DlPoint::MakeXY(10, 300),
                                TextRenderOptions{
                                    .font_size = font_size,
                                })
             .ok()) {
      return nullptr;
    }
    return builder.Build();
  };

  std::unique_ptr<impeller::testing::Screenshot> right =
      MakeScreenshot(callback("h", 0.444));
  if (!right) {
    GTEST_SKIP() << "making screenshots not supported.";
  }
  std::unique_ptr<impeller::testing::Screenshot> left =
      MakeScreenshot(callback("e", 0.444));

  int32_t left_max_y = CalculateMaxY(left.get());
  int32_t right_max_y = CalculateMaxY(right.get());
  int32_t y_diff = std::abs(left_max_y - right_max_y);
  EXPECT_TRUE(y_diff <= 2) << "y diff: " << y_diff;
}

TEST_P(DlGoldenTest, MaintainsSpace) {
  SetWindowSize(impeller::ISize(1024, 200));
  impeller::Scalar font_size = 300;

  int32_t middle = 0;
  std::optional<int32_t> last_space;
  for (int i = 0; i <= 100; ++i) {
    Scalar scale = 0.440 + i / 1000.0;
    absl::StatusOr<DlRect> text_bounds_or;
    sk_sp<DisplayList> display_list;
    {
      DisplayListBuilder builder;
      DlPaint paint;
      paint.setColor(DlColor::ARGB(1, 0, 0, 0));
      builder.DrawPaint(paint);
      builder.Scale(scale, scale);
      text_bounds_or = RenderTextInCanvasSkia(
          &builder, "ui", "Roboto-Regular.ttf", DlPoint::MakeXY(10, 300),
          TextRenderOptions{
              .font_size = font_size,
          });
      display_list = builder.Build();
    }
    ASSERT_TRUE(text_bounds_or.ok());
    ASSERT_TRUE(display_list);
    std::unique_ptr<impeller::testing::Screenshot> right =
        MakeScreenshot(display_list);
    if (!right) {
      GTEST_SKIP() << "making screenshots not supported.";
    }
    middle = std::rint(scale * text_bounds_or->GetCenter().y);

    std::optional<int32_t> space = CalculateSpaceBetweenUI(right.get(), middle);
    ASSERT_TRUE(space.has_value());
    if (space.has_value() && last_space.has_value()) {
      int32_t diff = abs(*space - *last_space);
      EXPECT_TRUE(diff <= 1)
          << "i:" << i << " space:" << *space << " last_space:" << *last_space;
    }
    last_space = space;
  }
}

namespace {
struct LeftmostIntensity {
  int32_t x;
  int32_t value;
};

/// Returns the highest value in the green channel for leftmost column that
/// isn't all black.
LeftmostIntensity CalculateLeftmostIntensity(
    const impeller::testing::Screenshot* img) {
  LeftmostIntensity result = {.x = static_cast<int32_t>(img->GetWidth()),
                              .value = 0};
  const uint32_t* ptr = reinterpret_cast<const uint32_t*>(img->GetBytes());
  for (size_t i = 0; i < img->GetHeight(); ++i) {
    for (int32_t j = 0; j < static_cast<int32_t>(img->GetWidth()); ++j) {
      if (((*ptr & 0x00ffffff) != 0)) {
        if (j < result.x) {
          result.x = j;
          result.value = (*ptr & 0xff00) >> 8;
        } else if (j == result.x) {
          result.value =
              std::max(static_cast<int32_t>(*ptr & 0xff), result.value);
        }
      }
      ptr++;
    }
  }
  return result;
}
}  // namespace

// Checks that the left most edge of the glyph is fading out as we push
// it to the right by fractional pixels.
TEST_P(DlGoldenTest, Subpixel) {
  SetWindowSize(impeller::ISize(1024, 200));
  impeller::Scalar font_size = 200;
  auto callback = [&](Scalar offset_x) -> sk_sp<DisplayList> {
    DisplayListBuilder builder;
    DlPaint paint;
    paint.setColor(DlColor::ARGB(1, 0, 0, 0));
    builder.DrawPaint(paint);
    EXPECT_TRUE(RenderTextInCanvasSkia(&builder, "ui", "Roboto-Regular.ttf",
                                       DlPoint::MakeXY(offset_x, 180),
                                       TextRenderOptions{
                                           .font_size = font_size,
                                           .is_subpixel = true,
                                       })
                    .ok());
    return builder.Build();
  };

  LeftmostIntensity intensity[5];
  for (int i = 0; i <= 4; ++i) {
    Scalar offset = 10 + (i / 4.0);
    std::unique_ptr<impeller::testing::Screenshot> right =
        MakeScreenshot(callback(offset));
    if (!right) {
      GTEST_SKIP() << "making screenshots not supported.";
    }
    intensity[i] = CalculateLeftmostIntensity(right.get());
    ASSERT_NE(intensity[i].value, 0);
  }
  for (int i = 1; i < 5; ++i) {
    EXPECT_TRUE(intensity[i].x - intensity[i - 1].x == 1 ||
                intensity[i].value < intensity[i - 1].value)
        << i;
  }
  EXPECT_EQ(intensity[4].x - intensity[0].x, 1);
}

// Checks that the left most edge of the glyph is fading out as we push
// it to the right by fractional pixels.
TEST_P(DlGoldenTest, SubpixelScaled) {
  SetWindowSize(impeller::ISize(1024, 200));
  impeller::Scalar font_size = 200;
  Scalar scalar = 0.75;
  auto callback = [&](Scalar offset_x) -> sk_sp<DisplayList> {
    DisplayListBuilder builder;
    builder.Scale(scalar, scalar);
    DlPaint paint;
    paint.setColor(DlColor::ARGB(1, 0, 0, 0));
    builder.DrawPaint(paint);
    EXPECT_TRUE(RenderTextInCanvasSkia(&builder, "ui", "Roboto-Regular.ttf",
                                       DlPoint::MakeXY(offset_x, 180),
                                       TextRenderOptions{
                                           .font_size = font_size,
                                           .is_subpixel = true,
                                       })
                    .ok());
    return builder.Build();
  };

  LeftmostIntensity intensity[5];
  Scalar offset_fraction = 0.25 / scalar;
  for (int i = 0; i <= 4; ++i) {
    Scalar offset = 10 + (offset_fraction * i);
    std::unique_ptr<impeller::testing::Screenshot> right =
        MakeScreenshot(callback(offset));
    if (!right) {
      GTEST_SKIP() << "making screenshots not supported.";
    }
    intensity[i] = CalculateLeftmostIntensity(right.get());
    ASSERT_NE(intensity[i].value, 0);
  }
  for (int i = 1; i < 5; ++i) {
    EXPECT_TRUE(intensity[i].x - intensity[i - 1].x == 1 ||
                intensity[i].value < intensity[i - 1].value)
        << i;
  }
  EXPECT_EQ(intensity[4].x - intensity[0].x, 1);
}

// Checks that the left most edge of the glyph is fading out as we push
// it to the right by fractional pixels.
TEST_P(DlGoldenTest, SubpixelScaledTranslated) {
  SetWindowSize(impeller::ISize(1024, 200));
  impeller::Scalar font_size = 200;
  Scalar scalar = 0.75;
  auto callback = [&](Scalar offset_x) -> sk_sp<DisplayList> {
    DisplayListBuilder builder;
    builder.Scale(scalar, scalar);
    DlPaint paint;
    paint.setColor(DlColor::ARGB(1, 0, 0, 0));
    builder.DrawPaint(paint);
    builder.Translate(offset_x, 180);
    EXPECT_TRUE(RenderTextInCanvasSkia(&builder, "ui", "Roboto-Regular.ttf",
                                       DlPoint::MakeXY(0, 0),
                                       TextRenderOptions{
                                           .font_size = font_size,
                                           .is_subpixel = true,
                                       })
                    .ok());
    return builder.Build();
  };

  LeftmostIntensity intensity[5];
  Scalar offset_fraction = 0.25 / scalar;
  for (int i = 0; i <= 4; ++i) {
    Scalar offset = 10 + (offset_fraction * i);
    std::unique_ptr<impeller::testing::Screenshot> right =
        MakeScreenshot(callback(offset));
    if (!right) {
      GTEST_SKIP() << "making screenshots not supported.";
    }
    intensity[i] = CalculateLeftmostIntensity(right.get());
    ASSERT_NE(intensity[i].value, 0);
  }
  for (int i = 1; i < 5; ++i) {
    EXPECT_TRUE(intensity[i].x - intensity[i - 1].x == 1 ||
                intensity[i].value < intensity[i - 1].value)
        << i;
  }
  EXPECT_EQ(intensity[4].x - intensity[0].x, 1);
}

namespace {
std::shared_ptr<impeller::testing::MockCapabilities> MakeBlendTestCapabilities(
    const std::shared_ptr<const impeller::Capabilities>& capabilities,
    bool framebuffer_fetch = false,
    bool offscreen_msaa = true) {
  auto mock_capabilities =
      std::make_shared<impeller::testing::MockCapabilities>();
  EXPECT_CALL(*mock_capabilities, SupportsFramebufferFetch())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(::testing::Return(framebuffer_fetch));
  FLT_FORWARD(mock_capabilities, capabilities, GetDefaultColorFormat);
  FLT_FORWARD(mock_capabilities, capabilities, GetDefaultStencilFormat);
  FLT_FORWARD(mock_capabilities, capabilities, GetDefaultDepthStencilFormat);
  EXPECT_CALL(*mock_capabilities, SupportsOffscreenMSAA())
      .Times(::testing::AnyNumber())
      .WillRepeatedly(::testing::Return(offscreen_msaa &&
                                        capabilities->SupportsOffscreenMSAA()));
  FLT_FORWARD(mock_capabilities, capabilities, SupportsImplicitResolvingMSAA);
  FLT_FORWARD(mock_capabilities, capabilities, SupportsReadFromResolve);
  FLT_FORWARD(mock_capabilities, capabilities, SupportsSSBO);
  FLT_FORWARD(mock_capabilities, capabilities, SupportsCompute);
  FLT_FORWARD(mock_capabilities, capabilities, SupportsTextureToTextureBlits);
  FLT_FORWARD(mock_capabilities, capabilities, GetDefaultGlyphAtlasFormat);
  FLT_FORWARD(mock_capabilities, capabilities, SupportsTriangleFan);
  FLT_FORWARD(mock_capabilities, capabilities, SupportsDecalSamplerAddressMode);
  FLT_FORWARD(mock_capabilities, capabilities, SupportsPrimitiveRestart);
  FLT_FORWARD(mock_capabilities, capabilities, Supports32BitPrimitiveIndices);
  FLT_FORWARD(mock_capabilities, capabilities, NeedsPartitionedHostBuffer);
  FLT_FORWARD(mock_capabilities, capabilities,
              GetMaximumRenderPassAttachmentSize);
  FLT_FORWARD(mock_capabilities, capabilities, GetMinimumUniformAlignment);
  return mock_capabilities;
}

constexpr impeller::ISize kBlendTestSize(128, 128);

std::vector<uint8_t> ReadBlendTestTexture(
    const std::shared_ptr<impeller::Context>& context,
    const std::shared_ptr<impeller::Texture>& texture) {
  if (!texture) {
    return {};
  }
  const auto bytes =
      texture->GetTextureDescriptor().GetByteSizeOfBaseMipLevel();
  auto buffer = context->GetResourceAllocator()->CreateBuffer(
      {.storage_mode = impeller::StorageMode::kHostVisible,
       .size = bytes,
       .readback = true});
  auto command = context->CreateCommandBuffer();
  if (!buffer || !command) {
    return {};
  }
  auto blit = command->CreateBlitPass();
  if (!blit || !blit->AddCopy(texture, buffer) || !blit->EncodeCommands()) {
    return {};
  }
  fml::AutoResetWaitableEvent done;
  if (!context->GetCommandQueue()
           ->Submit({command},
                    [&](impeller::CommandBuffer::Status status) {
                      EXPECT_EQ(status,
                                impeller::CommandBuffer::Status::kCompleted);
                      done.Signal();
                    })
           .ok()) {
    return {};
  }
  done.Wait();
  buffer->Invalidate();
  const auto* pixels = buffer->OnGetContents();
  return {pixels, pixels + bytes};
}

enum class CoverageTarget { kSnapshot, kSingleSample, kMSAA, kRenderOnly };

std::vector<uint8_t> RenderSrcCoveragePixels(
    const std::shared_ptr<impeller::Context>& context,
    const sk_sp<DisplayList>& display_list,
    CoverageTarget target_type = CoverageTarget::kSnapshot) {
  impeller::AiksContext renderer(context,
                                 impeller::TypographerContextSkia::Make());
  constexpr auto kFormat = impeller::PixelFormat::kR8G8B8A8UNormInt;
  std::shared_ptr<impeller::Texture> texture;
  if (target_type != CoverageTarget::kSnapshot) {
    impeller::RenderTargetAllocator allocator(context->GetResourceAllocator());
    auto target =
        target_type == CoverageTarget::kMSAA
            ? allocator.CreateOffscreenMSAA(
                  *context, kBlendTestSize, 1, "MSAA coverage target",
                  impeller::RenderTarget::kDefaultColorAttachmentConfigMSAA,
                  std::nullopt, nullptr, nullptr, nullptr, kFormat)
            : allocator.CreateOffscreen(
                  *context, kBlendTestSize, 1, "Single-sample coverage target",
                  impeller::RenderTarget::kDefaultColorAttachmentConfig,
                  std::nullopt, nullptr, nullptr, kFormat);
    if (!target.IsValid()) {
      return {};
    }
    if (target_type == CoverageTarget::kRenderOnly) {
      auto color = target.GetColorAttachment(0);
      auto descriptor = color.texture->GetTextureDescriptor();
      descriptor.usage = impeller::TextureUsage::kRenderTarget;
      color.texture =
          context->GetResourceAllocator()->CreateTexture(descriptor);
      target.SetColorAttachment(color, 0);
    }
    if (!RenderToTarget(renderer.GetContentContext(), target, display_list,
                        DlRect::MakeSize(kBlendTestSize), true, false)) {
      return {};
    }
    texture = target.GetRenderTargetTexture();
  } else {
    texture = impeller::DisplayListToTexture(display_list, kBlendTestSize,
                                             renderer, true, false, kFormat);
  }
  return ReadBlendTestTexture(context, texture);
}

void CheckSrcCoveragePixels(const std::vector<uint8_t>& mask,
                            const std::vector<uint8_t>& backdrop,
                            const std::vector<uint8_t>& result,
                            DlColor ink) {
  ASSERT_EQ(
      mask.size(),
      static_cast<size_t>(kBlendTestSize.width * kBlendTestSize.height) * 4u);
  ASSERT_EQ(backdrop.size(), mask.size());
  ASSERT_EQ(result.size(), mask.size());
  const auto src = impeller::skia_conversions::ToColor(ink).Premultiply();
  const std::array source = {src.red, src.green, src.blue, src.alpha};
  size_t edges = 0;
  size_t interiors = 0;
  for (size_t i = 0; i < mask.size(); i += 4) {
    const float coverage = mask[i + 3] / 255.0f;
    edges += coverage > 0 && coverage < 1;
    interiors += coverage == 1;
    for (size_t channel = 0; channel < 4; channel++) {
      ASSERT_NEAR(result[i + channel],
                  backdrop[i + channel] * (1 - coverage) +
                      255.0f * source[channel] * coverage,
                  2.0f)
          << "Pixel " << i / 4 << ", channel " << channel;
    }
  }
  EXPECT_GT(edges, 0u);
  EXPECT_GT(interiors, 0u);
}

enum class CoverageMaterial { kText, kCircle, kStrokedCircle };
constexpr std::array kCoverageMaterials{CoverageMaterial::kText,
                                        CoverageMaterial::kCircle,
                                        CoverageMaterial::kStrokedCircle};

sk_sp<DisplayList> BuildSrcCoverageScene(
    CoverageMaterial material,
    DlColor ink,
    bool draw_background,
    bool draw_source,
    bool nested_layer = false,
    const DlColorFilter* filter = nullptr) {
  DisplayListBuilder builder;
  if (nested_layer) {
    auto layer_paint = DlPaint().setBlendMode(DlBlendMode::kSrc);
    builder.SaveLayer(DlRect::MakeLTRB(12, 10, 114, 111), &layer_paint);
  }
  if (draw_background) {
    const auto paint =
        DlPaint().setAntiAlias(false).setBlendMode(DlBlendMode::kSrc);
    builder.DrawRect(DlRect::MakeLTRB(12, 10, 57, 111),
                     DlPaint(paint).setColor(DlColor(0xA8DBAF93)));
    builder.DrawRect(DlRect::MakeLTRB(57, 10, 114, 111),
                     DlPaint(paint).setColor(DlColor(0xDB82BACD)));
  }
  if (draw_source) {
    builder.ClipRect(DlRect::MakeLTRB(19, 15, 97, 108));
    builder.ClipOval(DlRect::MakeXYWH(39, 34, 18, 21), DlClipOp::kDifference);
    // Pixel grid and clip replay are observable on a nonuniform backdrop.
    builder.Translate(0.25f, 0.375f);
    // Src selects the circle shader even on backends with UberSDF enabled.
    // On the transparent reference target, the opaque draw's alpha is coverage.
    const auto mode = draw_background || material != CoverageMaterial::kText
                          ? DlBlendMode::kSrc
                          : DlBlendMode::kSrcOver;
    auto paint = DlPaint()
                     .setColor(ink)
                     .setBlendMode(mode)
                     .setColorFilter(filter)
                     .setAntiAlias(true);
    switch (material) {
      case CoverageMaterial::kText:
        EXPECT_TRUE(RenderTextInCanvasSkia(
                        &builder, "O", "Roboto-Regular.ttf", DlPoint(24, 90),
                        TextRenderOptions{.font_size = 72,
                                          .color = ink,
                                          .blend_mode = mode,
                                          .color_filter = filter})
                        .ok());
        break;
      case CoverageMaterial::kCircle:
        builder.DrawCircle(DlPoint(58, 57), 28.25f, paint);
        break;
      case CoverageMaterial::kStrokedCircle:
        paint.setDrawStyle(DlDrawStyle::kStroke).setStrokeWidth(8.5f);
        builder.DrawOval(DlRect::MakeCircleBounds(DlPoint(58, 57), 28.25f),
                         paint);
        break;
    }
  }
  if (nested_layer) {
    builder.Restore();
  }
  return builder.Build();
}

}  // namespace

TEST_P(DlGoldenTest, ModulateMultipliesPremultipliedColorAndAlpha) {
  DisplayListBuilder builder;
  builder.DrawColor(DlColor::kWhite().withAlpha(128), DlBlendMode::kSrc);
  builder.DrawRect(DlRect::MakeXYWH(16, 16, 32, 32),
                   DlPaint()
                       .setAntiAlias(false)
                       .setColor(DlColor::RGBA(1.0f, 0.5f, 0.0f, 0.5f))
                       .setBlendMode(DlBlendMode::kModulate));

  const auto context = GetContext();
  impeller::AiksContext renderer(context,
                                 impeller::TypographerContextSkia::Make());
  const auto texture = impeller::DisplayListToTexture(
      builder.Build(), kBlendTestSize, renderer, true, false,
      impeller::PixelFormat::kR8G8B8A8UNormInt);
  const auto pixels = ReadBlendTestTexture(context, texture);
  ASSERT_EQ(
      pixels.size(),
      static_cast<size_t>(kBlendTestSize.width * kBlendTestSize.height) * 4u);
  const size_t pixel = 4 * (32 * kBlendTestSize.width + 32);
  EXPECT_NEAR(pixels[pixel], 64, 1);
  EXPECT_NEAR(pixels[pixel + 1], 32, 1);
  EXPECT_EQ(pixels[pixel + 2], 0);
  EXPECT_NEAR(pixels[pixel + 3], 64, 1);
}

TEST_P(DlGoldenTest, SrcCoverageMatchesReference) {
  const auto context = GetContext();
  const DlColor color(0xFF245D84);
  const auto backdrop = RenderSrcCoveragePixels(
      context,
      BuildSrcCoverageScene(CoverageMaterial::kText, color, true, false));
  for (auto shape : kCoverageMaterials) {
    SCOPED_TRACE(static_cast<int>(shape));
    const auto mask = RenderSrcCoveragePixels(
        context, BuildSrcCoverageScene(shape, color, false, true));
    for (uint8_t alpha : {uint8_t{255}, uint8_t{0}, uint8_t{128}}) {
      SCOPED_TRACE(static_cast<int>(alpha));
      const auto ink = color.withAlpha(alpha);
      const auto result = RenderSrcCoveragePixels(
          context, BuildSrcCoverageScene(shape, ink, true, true));
      CheckSrcCoveragePixels(mask, backdrop, result, ink);
    }
  }
}

TEST_P(DlGoldenTest, SrcCoveragePreservesStrokedGlyphExtent) {
  const auto fixture = OpenFixtureAsSkData("Roboto-Regular.ttf");
  ASSERT_TRUE(fixture);
  auto typeface = txt::GetDefaultFontManager()->makeFromData(fixture);
  ASSERT_TRUE(typeface);
  SkFont font(typeface, 48);
  auto blob = SkTextBlob::MakeFromString("O", font);
  ASSERT_TRUE(blob);
  auto text =
      DlTextImpeller::Make(impeller::MakeTextFrameFromTextBlobSkia(blob));

  const auto context = GetContext();
  const DlColor color(0xFF245D84);
  const auto draw_text = [&](DisplayListBuilder& builder, DlColor ink,
                             DlBlendMode mode) {
    builder.DrawText(text, 43, 77,
                     DlPaint()
                         .setColor(ink)
                         .setBlendMode(mode)
                         .setDrawStyle(DlDrawStyle::kStroke)
                         .setStrokeJoin(DlStrokeJoin::kRound)
                         .setStrokeWidth(48));
  };
  DisplayListBuilder mask_builder;
  draw_text(mask_builder, color, DlBlendMode::kSrcOver);
  const auto mask = RenderSrcCoveragePixels(context, mask_builder.Build());

  const DlColor background(0xA8DBAF93);
  DisplayListBuilder backdrop_builder;
  backdrop_builder.DrawColor(background, DlBlendMode::kSrc);
  const auto backdrop =
      RenderSrcCoveragePixels(context, backdrop_builder.Build());

  for (uint8_t alpha : {uint8_t{0}, uint8_t{128}}) {
    SCOPED_TRACE(static_cast<int>(alpha));
    const auto ink = color.withAlpha(alpha);
    DisplayListBuilder builder;
    builder.DrawColor(background, DlBlendMode::kSrc);
    draw_text(builder, ink, DlBlendMode::kSrc);
    const auto result = RenderSrcCoveragePixels(context, builder.Build());
    CheckSrcCoveragePixels(mask, backdrop, result, ink);
  }
}

TEST_P(DlGoldenTest, SrcCoverageWithForcedBlendPaths) {
  if (GetParam() != PlaygroundBackend::kMetal &&
      GetParam() != PlaygroundBackend::kMetalSDF) {
    GTEST_SKIP() << "This test needs a backend with capability overrides.";
  }
  const auto context = GetContext();
  const auto original = context->GetCapabilities();
  fml::ScopedCleanupClosure restore([&] {
    EXPECT_TRUE(SetCapabilities(
                    std::const_pointer_cast<impeller::Capabilities>(original))
                    .ok());
  });
  const DlColor color(0xFF245D84);
  const auto ink = color.withAlpha(128);
  const auto filter = DlColorFilter::MakeBlend(ink, DlBlendMode::kSrc);
  ASSERT_TRUE(filter);
  for (bool fetch : {true, false}) {
    if (fetch && !original->SupportsFramebufferFetch()) {
      continue;
    }
    SCOPED_TRACE(fetch);
    ASSERT_TRUE(
        SetCapabilities(MakeBlendTestCapabilities(original, fetch,
                                                  /*offscreen_msaa=*/false))
            .ok());
    for (bool nested : {false, true}) {
      SCOPED_TRACE(nested);
      const auto backdrop = RenderSrcCoveragePixels(
          context, BuildSrcCoverageScene(CoverageMaterial::kText, color, true,
                                         false, nested));
      for (auto material : kCoverageMaterials) {
        SCOPED_TRACE(static_cast<int>(material));
        const auto mask = RenderSrcCoveragePixels(
            context,
            BuildSrcCoverageScene(material, color, false, true, nested));
        // Filtered alpha must be classified before the draw chooses its route.
        const auto result = RenderSrcCoveragePixels(
            context,
            BuildSrcCoverageScene(material, color, true, true, nested,
                                  filter.get()),
            CoverageTarget::kRenderOnly);
        CheckSrcCoveragePixels(mask, backdrop, result, ink);
      }
    }
  }
}

#if IMPELLER_ENABLE_VULKAN
TEST_P(DlGoldenTest, SrcCoverageWithVulkanDestinationReads) {
  if (GetBackend() != PlaygroundBackend::kVulkan) {
    GTEST_SKIP()
        << "This test controls the Vulkan framebuffer fetch workaround.";
  }
  const auto context = GetContext();
  auto& caps = const_cast<impeller::CapabilitiesVK&>(
      static_cast<const impeller::CapabilitiesVK&>(
          *context->GetCapabilities()));
  const auto& context_vk = impeller::SurfaceContextVK::Cast(*context);
  const auto original = context_vk.GetParent()->GetWorkarounds();
  fml::ScopedCleanupClosure restore([&] { caps.ApplyWorkarounds(original); });
  const DlColor color(0xFF245D84);
  for (auto target : {CoverageTarget::kSingleSample, CoverageTarget::kMSAA}) {
    SCOPED_TRACE(static_cast<int>(target));
    for (bool fetch : {true, false}) {
      if (fetch && original.input_attachment_self_dependency_broken) {
        continue;
      }
      SCOPED_TRACE(fetch);
      auto workarounds = original;
      workarounds.input_attachment_self_dependency_broken = !fetch;
      caps.ApplyWorkarounds(workarounds);
      for (auto material : kCoverageMaterials) {
        SCOPED_TRACE(static_cast<int>(material));
        const auto backdrop = RenderSrcCoveragePixels(
            context, BuildSrcCoverageScene(material, color, true, false),
            target);
        const auto mask = RenderSrcCoveragePixels(
            context, BuildSrcCoverageScene(material, color, false, true),
            target);
        for (uint8_t alpha : {uint8_t{0}, uint8_t{128}}) {
          SCOPED_TRACE(static_cast<int>(alpha));
          const auto ink = color.withAlpha(alpha);
          const auto result = RenderSrcCoveragePixels(
              context, BuildSrcCoverageScene(material, ink, true, true),
              target);
          CheckSrcCoveragePixels(mask, backdrop, result, ink);
        }
      }
    }
  }
}
#endif

// Advanced blends on devices without framebuffer fetch are rendered in an
// offscreen subpass whose render target has an integral size. The texture
// coordinates used to sample the inputs must describe exactly the rect that
// render target covers; if they describe the fractional coverage instead, the
// contents are rescaled on every blend. Stacking blends makes that error
// accumulate into a visible drift, which is what this test checks. Lighten is
// idempotent, so any number of stacked blends must produce the same image as a
// single one.
//
// Regression test for https://github.com/flutter/flutter/issues/192980.
TEST_P(DlGoldenTest, StackedOffscreenAdvancedBlendsDoNotResample) {
  if (GetParam() != PlaygroundBackend::kMetal) {
    GTEST_SKIP()
        << "This backend doesn't yet support setting device capabilities.";
  }

  std::shared_ptr<const impeller::Capabilities> old_capabilities =
      GetContext()->GetCapabilities();
  ASSERT_TRUE(
      SetCapabilities(MakeBlendTestCapabilities(old_capabilities)).ok());
  // The playground context is shared across tests, so the mocked capabilities
  // have to be undone however this test exits.
  fml::ScopedCleanupClosure restore_capabilities([&]() {
    EXPECT_TRUE(SetCapabilities(std::const_pointer_cast<impeller::Capabilities>(
                                    old_capabilities))
                    .ok());
  });

  auto draw = [](DlCanvas* canvas, int blend_count) {
    canvas->DrawColor(DlColor(0xFF404040));
    // The per-draw blend path computes its coverage from the whole pass, so
    // the coverage is only fractional through floating point error in the
    // transform. A 0.4x scale (which the ColorFilterAdvancedBlendNoFbFetch
    // golden also uses) does not round trip exactly through its inverse and
    // leaves a 2048 pixel pass at 2047.99988. A pure scale keeps the origin at
    // exactly zero, isolating the size mismatch from compositing at a
    // fractional origin.
    canvas->Scale(0.4f, 0.4f);

    // A backdrop with a hard vertical edge for the blends to resample.
    DlPaint backdrop;
    backdrop.setColor(DlColor(0xFFA0A0A0));
    canvas->DrawRect(DlRect::MakeLTRB(750, 0, 5120, 3840), backdrop);

    DlPaint blend;
    blend.setColor(DlColor(0xFF808080));
    blend.setBlendMode(DlBlendMode::kLighten);
    for (int i = 0; i < blend_count; ++i) {
      canvas->DrawRect(DlRect::MakeLTRB(250, 250, 1750, 1250), blend);
    }
  };

  auto make_screenshot = [&](int blend_count) {
    DisplayListBuilder builder;
    draw(&builder, blend_count);
    return MakeScreenshot(builder.Build());
  };

  std::unique_ptr<impeller::testing::Screenshot> once = make_screenshot(1);
  if (!once) {
    GTEST_SKIP() << "making screenshots not supported.";
  }
  std::unique_ptr<impeller::testing::Screenshot> stacked = make_screenshot(30);
  ASSERT_TRUE(stacked);

  double rmse = RMSE(once.get(), stacked.get());
  // Without the fix the thirty stacked blends drift the backdrop edge by
  // several pixels and the RMSE is about 5.75; with it the images are equal.
  EXPECT_LT(rmse, 1.0) << "rmse: " << rmse;
}

// The offscreen subpass of an advanced blend must cover exactly the pixels its
// coverage touches. The backdrop's coverage goes through the transform and its
// inverse, so for a 2048x1536 pass it lands slightly below or slightly above
// the integral size depending on the scale. Rounding the render target down
// drops the last column and row when it lands below, and rounding it up adds
// one when it lands above. Framebuffer fetch blends in place without a
// subpass, so its output is the reference the offscreen path has to match.
TEST_P(DlGoldenTest, OffscreenAdvancedBlendMatchesFramebufferFetch) {
  if (GetParam() != PlaygroundBackend::kMetal) {
    GTEST_SKIP()
        << "This backend doesn't yet support setting device capabilities.";
  }

  std::shared_ptr<const impeller::Capabilities> old_capabilities =
      GetContext()->GetCapabilities();
  if (!old_capabilities->SupportsFramebufferFetch()) {
    GTEST_SKIP() << "The reference image needs framebuffer fetch.";
  }
  std::shared_ptr<impeller::Capabilities> no_fb_fetch_capabilities =
      MakeBlendTestCapabilities(old_capabilities);
  // The playground context is shared across tests, so the mocked capabilities
  // have to be undone however this test exits.
  fml::ScopedCleanupClosure restore_capabilities([&]() {
    EXPECT_TRUE(SetCapabilities(std::const_pointer_cast<impeller::Capabilities>(
                                    old_capabilities))
                    .ok());
  });

  // A 0.4x scale leaves the pass coverage at 2047.99988x1535.99988 and a 0.43x
  // scale at 2048.00024x1536.00024.
  for (Scalar scale : {0.4f, 0.43f}) {
    SCOPED_TRACE(scale);
    DisplayListBuilder builder;
    builder.DrawColor(DlColor(0xFF404040), DlBlendMode::kSrcOver);
    builder.Scale(scale, scale);
    DlPaint backdrop;
    backdrop.setColor(DlColor(0xFFA0A0A0));
    builder.DrawRect(
        DlRect::MakeLTRB(300 / scale, 0, 2048 / scale, 1536 / scale), backdrop);
    // A blend that reaches into the last column and row of the pass.
    DlPaint blend;
    blend.setColor(DlColor(0xFF20C020));
    blend.setBlendMode(DlBlendMode::kLighten);
    builder.DrawRect(DlRect::MakeLTRB(100 / scale, 100 / scale, 2047.5f / scale,
                                      1535.5f / scale),
                     blend);
    sk_sp<DisplayList> display_list = builder.Build();

    ASSERT_TRUE(SetCapabilities(std::const_pointer_cast<impeller::Capabilities>(
                                    old_capabilities))
                    .ok());
    std::unique_ptr<impeller::testing::Screenshot> reference =
        MakeScreenshot(display_list);
    if (!reference) {
      GTEST_SKIP() << "making screenshots not supported.";
    }

    ASSERT_TRUE(SetCapabilities(no_fb_fetch_capabilities).ok());
    std::unique_ptr<impeller::testing::Screenshot> offscreen =
        MakeScreenshot(display_list);
    ASSERT_TRUE(offscreen);

    double rmse = RMSE(reference.get(), offscreen.get());
    EXPECT_LT(rmse, 0.1) << "rmse: " << rmse;
  }
}

}  // namespace testing
}  // namespace flutter
