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

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  TEST_START("Terminal Application Integration");

  test_run_lua_file_valid();
  test_run_lua_file_missing();

  TEST_END();
}
