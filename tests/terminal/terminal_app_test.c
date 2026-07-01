// Terminal Application Integration Tests
// Tests terminal_run_lua_file() directly without a GL context.

#include "test_framework.h"
#include "../ui.h"
#include "../examples/terminal/vgat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations from terminal source files
extern const vgat_cmd_t g_cmds[];

// Test: terminal_run_lua_file with a valid script
void test_run_lua_file_valid(void) {
  TEST("terminal_run_lua_file with valid script");
#if defined(HAVE_LUA)
  vgat_state_t *st = calloc(1, sizeof(vgat_state_t));
  ASSERT_NOT_NULL(st);
  vgat_screen_init(&st->screen, 24, 80);

  bool result = terminal_run_lua_file(st, "tests/test_simple.lua");
  ASSERT_TRUE(result);
  ASSERT_TRUE(st->screen.cursor_row > 0);

  vgat_screen_shutdown(&st->screen);
  if (st->L) lua_close(st->L);
  free(st);
#else
  SKIP("Lua not available");
#endif
  PASS();
}

// Test: terminal_run_lua_file with a missing script
void test_run_lua_file_missing(void) {
  TEST("terminal_run_lua_file with missing script");
#if defined(HAVE_LUA)
  vgat_state_t *st = calloc(1, sizeof(vgat_state_t));
  ASSERT_NOT_NULL(st);
  vgat_screen_init(&st->screen, 24, 80);

  bool result = terminal_run_lua_file(st, "tests/nonexistent.lua");
  ASSERT_FALSE(result);
  // Error was written to screen, cursor_row shows output
  ASSERT_TRUE(st->screen.cursor_row > 0);

  vgat_screen_shutdown(&st->screen);
  if (st->L) lua_close(st->L);
  free(st);
#else
  SKIP("Lua not available");
#endif
  PASS();
}

// Test: max_row tracks the highest row written via cursor_position
void test_max_row_tracking(void) {
  TEST("max_row tracking via cursor_position");
  vgat_screen screen;
  vgat_screen_init(&screen, 24, 80);

  // After init, max_row is 0
  ASSERT_EQUAL(screen.max_row, 0);

  // Simulate nano: clear screen, position to row 0, write text
  vgat_screen_clear(&screen);
  ASSERT_EQUAL(screen.max_row, 0);

  // Position to row 5 and write — max_row should become 6 (0-indexed row + 1)
  vgat_screen_cursor_position(&screen, 5, 0);
  vgat_screen_write_cell(&screen, 'x', 7, 0);
  ASSERT_EQUAL(screen.max_row, 6);

  // Position to row 0 — max_row should stay 6 (doesn't shrink)
  vgat_screen_cursor_position(&screen, 0, 0);
  ASSERT_EQUAL(screen.max_row, 6);

  // Position to row 23 — max_row should become 24
  vgat_screen_cursor_position(&screen, 23, 0);
  ASSERT_EQUAL(screen.max_row, 24);

  // Verify render: with written=24 and content_rows=24, first=0 (all rows visible)
  int written = screen.max_row;
  int content_rows = 24;
  int first = written - content_rows;
  if (first < 0) first = 0;
  ASSERT_EQUAL(first, 0);

  // Verify render: with written=24 and content_rows=20, first=4 (last 20 rows)
  written = screen.max_row;
  content_rows = 20;
  first = written - content_rows;
  if (first < 0) first = 0;
  ASSERT_EQUAL(first, 4);

  vgat_screen_shutdown(&screen);
  PASS();
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  TEST_START("Terminal Application Integration");

  test_run_lua_file_valid();
  test_run_lua_file_missing();
  test_max_row_tracking();

  TEST_END();
}
