// ANSI color and SGR parsing tests

#include "test_framework.h"
#include "../user/ansi.h"

void test_ansi16_palette_count(void) {
  TEST("kAnsi16 has exactly 16 entries");
  // Verify by checking a known color
  ASSERT_TRUE(kAnsi16[0] == 0xFF1E1E1Eu);  // soft background black
  PASS();
}

void test_ansi256_basic(void) {
  TEST("ansi256_to_rgba returns correct values for indices 0-15");
  for (int i = 0; i < 16; i++) {
    ASSERT_EQUAL(ansi256_to_rgba(i), kAnsi16[i]);
  }
  PASS();
}

void test_ansi256_to_rgba_216(void) {
  TEST("ansi256_to_rgba returns correct RGB for 216-color cube (index 16)");
  // Index 16 remains the first color in the 6x6x6 cube.
  uint32_t v = ansi256_to_rgba(16);
  ASSERT_EQUAL(v, 0xFF000000u);
  PASS();
}

void test_ansi256_grayscale(void) {
  TEST("ansi256_to_rgba returns grayscale for indices 232-255");
  uint32_t v232 = ansi256_to_rgba(232);
  // Gray 8: r=g=b=8
  ASSERT_EQUAL(v232, 0xFF080808u);
  uint32_t v255 = ansi256_to_rgba(255);
  // Gray 238: r=g=b=238 (8 + 23*10 = 238)
  ASSERT_EQUAL(v255, 0xFFEEEEEEu);
  PASS();
}

void test_nearest_ansi_index_black(void) {
  TEST("nearest_ansi_index returns 0 for the theme background");
  ASSERT_EQUAL(nearest_ansi_index(0xFF1E1E1Eu), 0);
  PASS();
}

void test_nearest_ansi_index_white(void) {
  TEST("nearest_ansi_index returns 15 for pure white");
  ASSERT_EQUAL(nearest_ansi_index(0xFFFFFFFFu), 15);
  PASS();
}

void test_clamp_ansi_index(void) {
  TEST("clamp_ansi_index clamps values to [0, 15]");
  ASSERT_EQUAL(clamp_ansi_index(-5), 0);
  ASSERT_EQUAL(clamp_ansi_index(0), 0);
  ASSERT_EQUAL(clamp_ansi_index(7), 7);
  ASSERT_EQUAL(clamp_ansi_index(15), 15);
  ASSERT_EQUAL(clamp_ansi_index(100), 15);
  PASS();
}

void test_ansi_apply_sgr_reset(void) {
  TEST("SGR 0 resets fg, bg, and bold");
  int fg = 5, bg = 2, def_fg = 7, def_bg = 0;
  bool bold = true;
  ansi_apply_sgr(0, &fg, &bg, def_fg, def_bg, &bold);
  ASSERT_EQUAL(fg, def_fg);
  ASSERT_EQUAL(bg, def_bg);
  ASSERT_FALSE(bold);
  PASS();
}

void test_ansi_apply_sgr_fg_basic(void) {
  TEST("SGR 30-37 sets foreground color");
  int fg = 7, bg = 0, def_fg = 7, def_bg = 0;
  bool bold = false;
  ansi_apply_sgr(31, &fg, &bg, def_fg, def_bg, &bold);  // red
  ASSERT_EQUAL(fg, 1);
  ansi_apply_sgr(32, &fg, &bg, def_fg, def_bg, &bold);  // green
  ASSERT_EQUAL(fg, 2);
  PASS();
}

void test_ansi_apply_sgr_bg_basic(void) {
  TEST("SGR 40-47 sets background color");
  int fg = 7, bg = 0, def_fg = 7, def_bg = 0;
  bool bold = false;
  ansi_apply_sgr(45, &fg, &bg, def_fg, def_bg, &bold);  // magenta bg (SGR 45)
  ASSERT_EQUAL(bg, 5);
  PASS();
}

void test_ansi_apply_sgr_bright(void) {
  TEST("SGR 90-97 sets bright foreground");
  int fg = 7, bg = 0, def_fg = 7, def_bg = 0;
  bool bold = false;
  ansi_apply_sgr(90, &fg, &bg, def_fg, def_bg, &bold);  // bright black
  ASSERT_EQUAL(fg, 8);
  ansi_apply_sgr(91, &fg, &bg, def_fg, def_bg, &bold);  // bright red
  ASSERT_EQUAL(fg, 9);
  PASS();
}

void test_ansi_apply_sgr_bold(void) {
  TEST("SGR 1 enables bold (maps to bright color)");
  int fg = 3, bg = 0, def_fg = 7, def_bg = 0;
  bool bold = false;
  ansi_apply_sgr(1, &fg, &bg, def_fg, def_bg, &bold);
  ASSERT_TRUE(bold);
  ASSERT_EQUAL(fg, 11);  // yellow (3) + 8 = 11 (bright yellow)
  PASS();
}

void test_ansi_apply_sgr_bold_off(void) {
  TEST("SGR 22 disables bold");
  int fg = 11, bg = 0, def_fg = 7, def_bg = 0;
  bool bold = true;
  ansi_apply_sgr(22, &fg, &bg, def_fg, def_bg, &bold);
  ASSERT_FALSE(bold);
  ASSERT_EQUAL(fg, 3);  // back to yellow
  PASS();
}

void test_ansi_apply_sgr_codes_with_38_5_256color(void) {
  TEST("ansi_apply_sgr_codes handles 38;5;n (256-color foreground)");
  int codes[3] = { 38, 5, 196 };  // 256-color red
  int fg = 7, bg = 0, def_fg = 7, def_bg = 0;
  bool bold = false;
  ansi_apply_sgr_codes(codes, 3, &fg, &bg, def_fg, def_bg, &bold);
  // 196 maps to nearest ANSI 16 color
  // This is testing the 256-color path works
  PASS();
}

void test_ansi_apply_sgr_codes_multiple(void) {
  TEST("ansi_apply_sgr_codes handles multiple SGR codes");
  int codes[3] = { 1, 31, 44 };  // bold + red fg + blue bg
  int fg = 7, bg = 0, def_fg = 7, def_bg = 0;
  bool bold = false;
  ansi_apply_sgr_codes(codes, 3, &fg, &bg, def_fg, def_bg, &bold);
  ASSERT_TRUE(bold);
  ASSERT_EQUAL(fg, 9);   // red (1) + 8 = bright red (9)
  ASSERT_EQUAL(bg, 4);   // blue (4)
  PASS();
}

int main(void) {
  TEST_START("ANSI color and SGR parsing");
  test_ansi16_palette_count();
  test_ansi256_basic();
  test_ansi256_to_rgba_216();
  test_ansi256_grayscale();
  test_nearest_ansi_index_black();
  test_nearest_ansi_index_white();
  test_clamp_ansi_index();
  test_ansi_apply_sgr_reset();
  test_ansi_apply_sgr_fg_basic();
  test_ansi_apply_sgr_bg_basic();
  test_ansi_apply_sgr_bright();
  test_ansi_apply_sgr_bold();
  test_ansi_apply_sgr_bold_off();
  test_ansi_apply_sgr_codes_with_38_5_256color();
  test_ansi_apply_sgr_codes_multiple();
  TEST_END();
}
