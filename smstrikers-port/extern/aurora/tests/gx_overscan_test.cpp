// smstrikers-port: GXAdjustForOverscan has two answers, and which one a game
// gets depends on whether it pinned the render target to the VI mode.
#include <gtest/gtest.h>

#include <dolphin/gx.h>

namespace aurora::window::testing {
extern float g_frameBufferScale;
}

namespace {

struct OverscanTest : ::testing::Test {
  void TearDown() override { aurora::window::testing::g_frameBufferScale = 0.f; }
};

// Stock Aurora: the framebuffer is the window, whatever the mode says.
TEST_F(OverscanTest, ReportsTheWindowWhenTheRenderTargetIsNotPinned) {
  aurora::window::testing::g_frameBufferScale = 0.f;
  GXRenderModeObj out{};
  GXAdjustForOverscan(&GXNtsc480Int, &out, 8, 16);
  EXPECT_EQ(out.fbWidth, 640);
  EXPECT_EQ(out.efbHeight, 480);
  EXPECT_EQ(out.xfbHeight, 480);
  EXPECT_EQ(out.viWidth, GXNtsc480Int.viWidth);
  EXPECT_EQ(out.viXOrigin, GXNtsc480Int.viXOrigin);
}

// Pinned: the SDK's own arithmetic. NTSC interlaced, double-strike field buffer,
// so the vertical trim is applied whole to every height.
TEST_F(OverscanTest, DoesTheSdkArithmeticWhenTheRenderTargetIsPinned) {
  aurora::window::testing::g_frameBufferScale = 1.f;
  GXRenderModeObj out{};
  GXAdjustForOverscan(&GXNtsc480Int, &out, 8, 16);
  EXPECT_EQ(out.fbWidth, 640 - 16);
  EXPECT_EQ(out.efbHeight, 480 - 32);
  EXPECT_EQ(out.xfbHeight, 480 - 32);
  EXPECT_EQ(out.viWidth, 640 - 16);
  EXPECT_EQ(out.viHeight, 480 - 32);
  EXPECT_EQ(out.viXOrigin, GXNtsc480Int.viXOrigin + 8);
  EXPECT_EQ(out.viYOrigin, GXNtsc480Int.viYOrigin + 16);
  EXPECT_EQ(out.xFBmode, GXNtsc480Int.xFBmode);
}

// A single-strike field buffer at 240 lines trims the XFB by half the amount,
// which is the one place the SDK's formula is not uniform.
TEST_F(OverscanTest, HalvesTheXfbTrimForASingleStrikeFieldBuffer) {
  aurora::window::testing::g_frameBufferScale = 1.f;
  GXRenderModeObj in = GXNtsc480Int;
  in.xFBmode = VI_XFBMODE_SF;
  in.xfbHeight = 240;
  GXRenderModeObj out{};
  GXAdjustForOverscan(&in, &out, 0, 16);
  EXPECT_EQ(out.xfbHeight, 240 - 16);
  EXPECT_EQ(out.efbHeight, 480 - (32 * 480) / 240);
}

} // namespace
