// builtin_scrollbar_test.c — Regression tests for built-in window scrollbars.
//
// Covers scrollbar thumb hit-testing and drag stability when the owning
// window updates its own scroll position in response to evVScroll.

#include "test_framework.h"
#include "test_env.h"
#include "../ui.h"

static lresult_t scrolling_window_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam) {
  (void)lparam;
  if (msg == evCreate || msg == evDestroy || msg == evPaint) return 1;
  if (msg == evHScroll) {
    win->hscroll.pos = (uint16_t)wparam;
    return 1;
  }
  if (msg == evVScroll) {
    win->vscroll.pos = (uint16_t)wparam;
    return 1;
  }
  return 0;
}

static window_t *make_scrolling_window(int w, int h) {
  irect16_t fr = {0, 0, w, h};
  return create_window("scrolling", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
                       &fr, NULL, scrolling_window_proc, 0, NULL);
}

static void set_vscroll(window_t *win, int min_val, int max_val, int page, int pos) {
  scroll_info_t info = {
    .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
    .nMin = min_val,
    .nMax = max_val,
    .nPage = page,
    .nPos = pos,
  };
  set_scroll_info(win, SB_VERT, &info, false);
}

static void set_hscroll(window_t *win, int min_val, int max_val, int page, int pos) {
  scroll_info_t info = {
    .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
    .nMin = min_val,
    .nMax = max_val,
    .nPage = page,
    .nPos = pos,
  };
  set_scroll_info(win, SB_HORZ, &info, false);
}

static window_t *make_hscrolling_window(int w, int h) {
  irect16_t fr = {0, 0, w, h};
  return create_window("hscrolling", WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_HSCROLL,
                       &fr, NULL, scrolling_window_proc, 0, NULL);
}

void test_builtin_vscroll_click_starts_drag(void) {
  TEST("built-in vscroll: clicking the thumb starts a drag");

  test_env_init();
  window_t *win = make_scrolling_window(100, 100);
  ASSERT_NOT_NULL(win);

  set_vscroll(win, 0, 200, 20, 20);

  // With arrow buttons, the thumb is visually at y = 20. Dispatch reports
  // client coordinates with the current scroll offset added.
  send_message(win, evLeftButtonDown, MAKEDWORD(95, 22 + 20), NULL);

  ASSERT_TRUE(win->vscroll.dragging);
  ASSERT_EQUAL(win->vscroll.drag_start_mouse, 9);
  ASSERT_EQUAL(win->vscroll.drag_mouse, 9);
  ASSERT_EQUAL((int)win->vscroll.pos, 20);

  send_message(win, evLeftButtonUp, MAKEDWORD(95, 22 + 20), NULL);
  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_builtin_vscroll_drag_ignores_scroll_feedback(void) {
  TEST("built-in vscroll: drag uses mouse delta, not scroll feedback");

  test_env_init();
  window_t *win = make_scrolling_window(100, 100);
  ASSERT_NOT_NULL(win);

  set_vscroll(win, 0, 200, 20, 20);

  // Start on the thumb.
  send_message(win, evLeftButtonDown, MAKEDWORD(95, 22 + 20), NULL);
  ASSERT_TRUE(win->vscroll.dragging);

  // Move the mouse by 1 px. The scrollbar changes its position by 2.
  send_message(win, evMouseMove, MAKEDWORD(95, 23 + 20), (void *)(intptr_t)MAKEDWORD(0, 1));
  ASSERT_EQUAL((int)win->vscroll.pos, 22);
  ASSERT_EQUAL((int)win->vscroll.pos, 22);

  // The window's scroll position changed, so the next LOCAL_Y reported by the
  // framework would also shift by +2 even if the cursor stayed still.
  // A correct drag handler must ignore that feedback and leave the thumb at 22.
  send_message(win, evMouseMove, MAKEDWORD(95, 23 + 22), (void *)(intptr_t)MAKEDWORD(0, 0));
  ASSERT_EQUAL((int)win->vscroll.pos, 22);
  ASSERT_EQUAL((int)win->vscroll.pos, 22);

  send_message(win, evLeftButtonUp, MAKEDWORD(95, 23 + 22), NULL);
  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_builtin_hscroll_hit_test_ignores_scroll_feedback(void) {
  TEST("built-in hscroll: hit-test uses visual coords after scrolling");

  test_env_init();
  window_t *win = make_hscrolling_window(100, 100);
  ASSERT_NOT_NULL(win);

  set_hscroll(win, 0, 200, 20, 50);

  // Platform dispatch adds the current scroll position to LOCAL_X. Built-in
  // scrollbar chrome is fixed on screen, so handlers must subtract it back out.
  send_message(win, evLeftButtonDown, MAKEDWORD(95 + 50, 90), NULL);
  ASSERT_EQUAL((int)win->hscroll.pos, 51);

  set_hscroll(win, 0, 200, 20, 50);
  send_message(win, evLeftButtonDown, MAKEDWORD(35 + 50, 90), NULL);
  ASSERT_TRUE(win->hscroll.dragging);
  ASSERT_EQUAL(win->hscroll.drag_start_mouse, 22);
  ASSERT_EQUAL((int)win->hscroll.pos, 50);

  send_message(win, evLeftButtonUp, MAKEDWORD(35 + 50, 90), NULL);
  destroy_window(win);
  test_env_shutdown();
  PASS();
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  TEST_START("built-in scrollbar tests");

  test_builtin_vscroll_click_starts_drag();
  test_builtin_vscroll_drag_ignores_scroll_feedback();
  test_builtin_hscroll_hit_test_ignores_scroll_feedback();

  TEST_END();
}
