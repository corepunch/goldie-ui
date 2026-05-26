// Multi-line text edit control (analogous to WinAPI ES_MULTILINE edit).
//
// State is stored in win->userdata (me_state_t, heap-allocated at create time).
// Text lives in buf[ME_BUF_SIZE]; win->title is not used for content.
//
// Messages handled in addition to the standard window messages:
//   edGetText  wparam=buf_size, lparam=char* → copies text, returns byte count
//   edSetText  wparam=0, lparam=const char* → replaces text, resets cursor/scroll

#include <string.h>
#include <stdlib.h>

#include "../user/user.h"
#include "../user/messages.h"
#include "../user/draw.h"

#define ME_BUF_SIZE 2048
#define ME_PADDING  3
#define ME_MIN_WIDTH  80
#define ME_FONT FONT_SMALL
// Maximum number of characters that can be stored (leave room for the NUL).
#define ME_MAX_LEN  (ME_BUF_SIZE - 2)

extern window_t *get_root_window(window_t *window);
extern int titlebar_height(window_t const *win);

typedef struct {
  char buf[ME_BUF_SIZE];
  int  len;        // strlen(buf)
  int  cursor;     // byte offset of caret in buf
  int  scroll_y;   // vertical scroll in pixels
  bool multiline;  // false when serving the TextEdit wrapper
} me_state_t;

static int me_line_height(void) {
  return text_char_height(ME_FONT);
}

static int me_char_height(void) {
  return text_char_height(ME_FONT);
}

static int me_char_advance(unsigned char c) {
  return text_char_width(ME_FONT, c);
}

// ---------------------------------------------------------------------------
// Internal layout helpers
// ---------------------------------------------------------------------------

// Advance one character's worth of wrapping state.  Uses the same algorithm
// as draw_text_wrapped() / calc_text_height() in user/text.c so that cursor
// positions exactly match rendered glyph positions.  Mutates *cx/*cy.
static void me_advance(me_state_t const *s, int i, int max_w, int *cx, int *cy) {
  const char *buf = s->buf;
  unsigned char c = (unsigned char)buf[i];
  if (c == '\n') {
    *cx = 0;
    *cy += me_line_height();
  } else if (c == ' ') {
    *cx += me_char_advance(c);
  } else {
    int cw = me_char_advance(c);
    if (s->multiline && cw > 0 && *cx + cw > max_w) {
      *cx = 0;
      *cy += me_line_height();
    }
    *cx += cw;
  }
}

// Compute visual (cx, cy) of byte offset `cursor` in buf.
// Coordinates are relative to the text-area origin (0, 0).
// out_x may be NULL when only the row position is needed.
static void me_cursor_xy(me_state_t const *s, int max_w, int *out_x, int *out_y) {
  int cx = 0, cy = 0;
  for (int i = 0; i < s->cursor && s->buf[i]; i++)
    me_advance(s, i, max_w, &cx, &cy);
  if (out_x) *out_x = cx;
  *out_y = cy;
}

// Find byte offset whose visual position is closest to pixel (tx, ty).
static int me_find_at_xy(me_state_t const *s, int tx, int ty, int max_w) {
  int cx = 0, cy = 0;
  int best = 0, best_d = 0x7fffffff;

  // Check position 0.
  if (cy == ty) {
    int d = abs(cx - tx);
    if (d < best_d) { best_d = d; best = 0; }
  }

  for (int i = 0; i < s->len; i++) {
    me_advance(s, i, max_w, &cx, &cy);
    if (cy == ty) {
      int d = abs(cx - tx);
      if (d < best_d) { best_d = d; best = i + 1; }
    }
    if (cy > ty) break;
  }
  return best;
}

// Scroll so that the caret is within the visible viewport.
static void me_ensure_visible(me_state_t *s, int max_w, int vis_h) {
  int cy;
  if (!s->multiline) {
    s->scroll_y = 0;
    return;
  }
  me_cursor_xy(s, max_w, NULL, &cy);
  if (cy < s->scroll_y)
    s->scroll_y = cy;
  if (cy + me_line_height() > s->scroll_y + vis_h)
    s->scroll_y = cy + me_line_height() - vis_h;
  if (s->scroll_y < 0) s->scroll_y = 0;
}

// Text-area dimensions in client pixels (scrollbar strip excluded when visible).
// Uses win->frame.w directly (not get_client_rect) because multiedit draws into
// the full frame without a title bar; only the vertical scrollbar strip is carved out.
static void me_text_dims(window_t *win, int *tw, int *th) {
  bool has_v = (win->flags & WINDOW_VSCROLL) && win->vscroll.visible;
  me_state_t *s = (me_state_t *)win->userdata;
  int pad_x = (s && !s->multiline) ? TEXTEDIT_PADDING_HORZ : ME_PADDING;
  int pad_y = (s && !s->multiline) ? TEXTEDIT_PADDING_VERT : ME_PADDING;
  *tw = win->frame.w - (has_v ? SCROLLBAR_WIDTH : 0) - pad_x * 2;
  *th = win->frame.h - pad_y * 2;
  if (*tw < 1) *tw = 1;
  if (*th < 1) *th = 1;
}

// Synchronise the built-in vertical scrollbar with the current text height.
// Only does anything when WINDOW_VSCROLL is set.  The bar remains permanently
// visible (forced by show_scroll_bar in evCreate) but is disabled/greyed out
// when the entire text fits in the viewport without scrolling.
static void me_sync_scrollbar(window_t *win, me_state_t *s) {
  if (!s->multiline) return;
  if (!(win->flags & WINDOW_VSCROLL)) return;
  int tw, th;
  me_text_dims(win, &tw, &th);
  int total_h = calc_text_height(s->buf, tw);
  bool needs_scroll = (total_h > th);
  int max_scroll = needs_scroll ? (total_h - th) : 0;
  if (s->scroll_y > max_scroll) s->scroll_y = max_scroll;
  if (s->scroll_y < 0)         s->scroll_y = 0;
  scroll_info_t si = {
    .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
    .nMin  = 0,
    .nMax  = (total_h > th) ? total_h : th,
    .nPage = (uint32_t)th,
    .nPos  = s->scroll_y,
  };
  set_scroll_info(win, SB_VERT, &si, false);
  enable_scroll_bar(win, SB_VERT, needs_scroll);
}

static void me_sync_window_fields(window_t *win, me_state_t const *s) {
  win->cursor_pos = (uint32_t)s->cursor;
  if (!s->multiline) {
    strncpy(win->title, s->buf, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';
  }
}

// Compute the absolute screen rect of win's text area.
// Walks the parent chain so the result is correct even when win is nested
// inside an intermediate container window (not a direct child of root).
static irect16_t me_text_screen_rect(window_t *win, window_t *root) {
  int tw, th;
  me_text_dims(win, &tw, &th);
  me_state_t *s = (me_state_t *)win->userdata;
  int pad_x = (s && !s->multiline) ? TEXTEDIT_PADDING_HORZ : ME_PADDING;
  int pad_y = (s && !s->multiline) ? TEXTEDIT_PADDING_VERT : ME_PADDING;
  int x = win->frame.x + pad_x;
  int y = win->frame.y + pad_y;
  for (window_t *p = win->parent; p && p != root; p = p->parent) {
    x += p->frame.x;
    y += p->frame.y;
  }
  return (irect16_t){
    root->frame.x + x,
    root->frame.y + titlebar_height(root) + y,
    tw,
    th,
  };
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

lresult_t win_multiedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  me_state_t *s = (me_state_t *)win->userdata;

  switch (msg) {
    // ── Create ─────────────────────────────────────────────────────────────
    case evCreate: {
      s = (me_state_t *)allocate_window_data(win, sizeof(me_state_t));
      if (!s) return true;
      s->multiline = (wparam == 0);
      if (s->multiline)
        win->flags |= WINDOW_FLEXSPACE;
      strncpy(s->buf, win->title, ME_BUF_SIZE - 1);
      s->buf[ME_BUF_SIZE - 1] = '\0';
      s->len      = (int)strlen(s->buf);
      s->cursor   = s->multiline ? s->len : 0;
      s->scroll_y = 0;
      me_sync_window_fields(win, s);
      // When WINDOW_VSCROLL is set, lock the bar permanently visible so it is
      // always shown (enabled/disabled to indicate whether scrolling is needed).
      if (win->flags & WINDOW_VSCROLL)
        show_scroll_bar(win, SB_VERT, true);
      me_sync_scrollbar(win, s);
      return true;
    }

    case evMeasure: {
      layout_measure_t *m = (layout_measure_t *)lparam;
      if (m) {
        if (s && !s->multiline) {
          m->desired_w = MAX(ME_MIN_WIDTH,
                             text_strwidth(FONT_SMALL, s->buf) + TEXTEDIT_PADDING_HORZ * 2);
          m->desired_h = text_char_height(FONT_SMALL) + TEXTEDIT_PADDING_VERT * 2;
          return true;
        }
        int avail_w = m->avail_w > 0 ? m->avail_w : ME_MIN_WIDTH;
        if (avail_w < ME_MIN_WIDTH) avail_w = ME_MIN_WIDTH;
        m->desired_w = MAX(ME_MIN_WIDTH,
                           text_strwidth(FONT_SMALL, win->title) + TEXTEDIT_PADDING_HORZ * 2);
        m->desired_h = MAX(text_char_height(FONT_SMALL) * 4,
                           calc_text_height_font(FONT_SMALL, s ? s->buf : win->title, avail_w) + ME_PADDING * 2);
      }
      return true;
    }

    // ── Destroy ────────────────────────────────────────────────────────────
    case evDestroy:
      free(s);
      win->userdata = NULL;
      return false;

    // ── Focus / blur ───────────────────────────────────────────────────────
    case evSetFocus:
    case evKillFocus:
      invalidate_window(win);
      return false;

    // ── Paint ──────────────────────────────────────────────────────────────
    case evPaint: {
      if (!s) return true;
      bool focused = (g_ui_runtime.focused == win);

      // Focus ring (matches win_textedit style).
      fill_rect(focused ? get_sys_color(brFocusRing)
                        : get_sys_color(brWindowBg),
                R(-1, -1, win->frame.w + 2, win->frame.h + 2));

      // Inset bevel border.
      draw_button((irect16_t){0, 0, win->frame.w, win->frame.h}, 1, 1, true);

      int tw, th;
      me_text_dims(win, &tw, &th);
      int tx = s->multiline ? ME_PADDING : TEXTEDIT_PADDING_HORZ;
      int ty = s->multiline ? ME_PADDING : (win->frame.h - me_char_height()) / 2;

      // Clip to text area (scissor uses absolute screen coordinates).
      window_t *root = get_root_window(win);
      irect16_t tr = me_text_screen_rect(win, root);
      set_clip_rect(NULL, tr);

      if (s->multiline) {
        // Draw wrapped text, offset upward by scroll_y.
        irect16_t vp = { tx, ty - s->scroll_y, tw, th + s->scroll_y };
        draw_text_wrapped(s->buf, &vp, get_sys_color(brTextNormal));
      } else {
        irect16_t text_rect = { tx, ty, tw, th };
        draw_text_clipped(FONT_SMALL, s->buf, &text_rect, get_sys_color(brTextNormal), 0);
      }

      // Draw caret when focused.
      if (focused && (s->multiline || window_has_state(win, WINDOW_STATE_EDITING))) {
        int cx, cy;
        me_cursor_xy(s, tw, &cx, &cy);
        int cur_y = ty + cy - s->scroll_y;
        if (cur_y >= ty - me_line_height() && cur_y < ty + th) {
          fill_rect(get_sys_color(brTextNormal),
                    R(tx + cx, cur_y, 2, me_char_height()));
        }
      }

      // Reset scissor to full control frame so subsequent rendering is unclipped.
      int pad_x = s->multiline ? ME_PADDING : TEXTEDIT_PADDING_HORZ;
      int pad_y = s->multiline ? ME_PADDING : TEXTEDIT_PADDING_VERT;
      set_clip_rect(NULL, (irect16_t){
        tr.x - pad_x, tr.y - pad_y,
        win->frame.w, win->frame.h,
      });

      return true;
    }

    // ── Mouse click to position ────────────────────────────────────────────
    case evLeftButtonUp: {
      if (!s) return true;
      int tw, th;
      me_text_dims(win, &tw, &th);
      // wparam carries client-local x (LOWORD) and y (HIWORD).
      if (!s->multiline && g_ui_runtime.focused == win)
        window_set_state(win, WINDOW_STATE_EDITING, true);
      int pad_x = s->multiline ? ME_PADDING : TEXTEDIT_PADDING_HORZ;
      int pad_y = s->multiline ? ME_PADDING : TEXTEDIT_PADDING_VERT;
      int lx = (int)(int16_t)LOWORD(wparam) - pad_x;
      int ly = (int)(int16_t)HIWORD(wparam) - pad_y + s->scroll_y;
      int lh = me_line_height();
      int target_y = s->multiline ? (ly / lh) * lh : 0;
      if (target_y < 0) target_y = 0;
      s->cursor = me_find_at_xy(s, lx, target_y, tw);
      me_ensure_visible(s, tw, th);
      me_sync_window_fields(win, s);
      invalidate_window(win);
      return true;
    }

    // ── Mouse-wheel scroll ─────────────────────────────────────────────────
    case evWheel: {
      if (!s) return true;
      int tw, th;
      me_text_dims(win, &tw, &th);
      // lparam = scroll deltas MAKEDWORD(dx, dy)
      int delta = -(int16_t)HIWORD((uintptr_t)lparam);
      s->scroll_y += delta;
      if (s->scroll_y < 0) s->scroll_y = 0;
      int total_h = calc_text_height(s->buf, tw);
      int max_scroll = total_h - th;
      if (max_scroll < 0) max_scroll = 0;
      if (s->scroll_y > max_scroll) s->scroll_y = max_scroll;
      me_sync_scrollbar(win, s);
      invalidate_window(win);
      return true;
    }

    // ── Text input ─────────────────────────────────────────────────────────
    case evTextInput: {
      if (!s || !lparam) return true;
      if (!s->multiline && !window_has_state(win, WINDOW_STATE_EDITING))
        return true;
      char c = *(const char *)lparam;
      // Accept only printable ASCII; newlines are handled via AX_KEY_ENTER.
      if ((unsigned char)c < 32 || (unsigned char)c > 126) return true;
      if (s->len >= ME_MAX_LEN) return true;
      memmove(s->buf + s->cursor + 1,
              s->buf + s->cursor,
              (size_t)(s->len - s->cursor + 1));
      s->buf[s->cursor] = c;
      s->cursor++;
      s->len++;
      me_sync_window_fields(win, s);
      int tw, th;
      me_text_dims(win, &tw, &th);
      me_ensure_visible(s, tw, th);
      me_sync_scrollbar(win, s);
      invalidate_window(win);
      return true;
    }

    // ── Key navigation and editing ─────────────────────────────────────────
    case evKeyDown: {
      if (!s) return true;
      int tw, th;
      me_text_dims(win, &tw, &th);
      switch (wparam) {

        case AX_KEY_ENTER:
          if (!s->multiline) {
            if (!window_has_state(win, WINDOW_STATE_EDITING)) {
              s->cursor = s->len;
              me_sync_window_fields(win, s);
              window_set_state(win, WINDOW_STATE_EDITING, true);
            } else {
              send_message(get_root_window(win), evCommand,
                           MAKEDWORD(win->id, edUpdate), win);
              window_set_state(win, WINDOW_STATE_EDITING, false);
            }
            invalidate_window(win);
            return true;
          }
          if (s->len < ME_MAX_LEN) {
            memmove(s->buf + s->cursor + 1,
                    s->buf + s->cursor,
                    (size_t)(s->len - s->cursor + 1));
            s->buf[s->cursor] = '\n';
            s->cursor++;
            s->len++;
            me_sync_window_fields(win, s);
            me_ensure_visible(s, tw, th);
            me_sync_scrollbar(win, s);
            invalidate_window(win);
          }
          return true;

        case AX_KEY_BACKSPACE:
          if (!s->multiline && !window_has_state(win, WINDOW_STATE_EDITING)) {
            invalidate_window(win);
            return true;
          }
          if (s->cursor > 0) {
            memmove(s->buf + s->cursor - 1,
                    s->buf + s->cursor,
                    (size_t)(s->len - s->cursor + 1));
            s->cursor--;
            s->len--;
            me_sync_window_fields(win, s);
            me_ensure_visible(s, tw, th);
            me_sync_scrollbar(win, s);
            invalidate_window(win);
          }
          return true;

        case AX_KEY_DEL:
          if (!s->multiline && !window_has_state(win, WINDOW_STATE_EDITING))
            return true;
          if (s->cursor < s->len) {
            memmove(s->buf + s->cursor,
                    s->buf + s->cursor + 1,
                    (size_t)(s->len - s->cursor));
            s->len--;
            me_sync_window_fields(win, s);
            me_ensure_visible(s, tw, th);
            me_sync_scrollbar(win, s);
            invalidate_window(win);
          }
          return true;

        case AX_KEY_LEFTARROW:
          if (!s->multiline && !window_has_state(win, WINDOW_STATE_EDITING)) {
            invalidate_window(win);
            return true;
          }
          if (s->cursor > 0) {
            s->cursor--;
            me_sync_window_fields(win, s);
            me_ensure_visible(s, tw, th);
            invalidate_window(win);
          }
          return true;

        case AX_KEY_RIGHTARROW:
          if (!s->multiline && !window_has_state(win, WINDOW_STATE_EDITING)) {
            invalidate_window(win);
            return true;
          }
          if (s->cursor < s->len) {
            s->cursor++;
            me_sync_window_fields(win, s);
            me_ensure_visible(s, tw, th);
            invalidate_window(win);
          }
          return true;

        case AX_KEY_UPARROW: {
          if (!s->multiline)
            return window_has_state(win, WINDOW_STATE_EDITING);
          int cx, cy;
          me_cursor_xy(s, tw, &cx, &cy);
          int ny = cy - me_line_height();
          if (ny >= 0) {
            s->cursor = me_find_at_xy(s, cx, ny, tw);
            me_sync_window_fields(win, s);
            me_ensure_visible(s, tw, th);
            invalidate_window(win);
          }
          return true;
        }

        case AX_KEY_DOWNARROW: {
          if (!s->multiline)
            return window_has_state(win, WINDOW_STATE_EDITING);
          int cx, cy;
          me_cursor_xy(s, tw, &cx, &cy);
          int ny = cy + me_line_height();
          int new_pos = me_find_at_xy(s, cx, ny, tw);
          if (new_pos != s->cursor) {
            s->cursor = new_pos;
            me_sync_window_fields(win, s);
            me_ensure_visible(s, tw, th);
            invalidate_window(win);
          }
          return true;
        }

        case AX_KEY_HOME: {
          if (!s->multiline)
            return window_has_state(win, WINDOW_STATE_EDITING);
          // Move to start of the current logical line (scan back to previous \n or buf start).
          int p = s->cursor;
          while (p > 0 && s->buf[p - 1] != '\n') p--;
          s->cursor = p;
          me_sync_window_fields(win, s);
          me_ensure_visible(s, tw, th);
          invalidate_window(win);
          return true;
        }

        case AX_KEY_END: {
          if (!s->multiline)
            return window_has_state(win, WINDOW_STATE_EDITING);
          // Move to end of the current logical line (forward to next \n or buf end).
          int p = s->cursor;
          while (p < s->len && s->buf[p] != '\n') p++;
          s->cursor = p;
          me_sync_window_fields(win, s);
          me_ensure_visible(s, tw, th);
          invalidate_window(win);
          return true;
        }

        case AX_KEY_TAB:
          if (!s->multiline) {
            if (window_has_state(win, WINDOW_STATE_EDITING)) {
              send_message(get_root_window(win), evCommand,
                           MAKEDWORD(win->id, edUpdate), win);
              window_set_state(win, WINDOW_STATE_EDITING, false);
              invalidate_window(win);
            }
            return false;
          }
          // Notify parent and yield focus so Tab advances to next control.
          send_message(get_root_window(win), evCommand,
                       MAKEDWORD(win->id, edUpdate), win);
          return false;

        case AX_KEY_ESCAPE:
          if (!s->multiline) {
            window_set_state(win, WINDOW_STATE_EDITING, false);
            invalidate_window(win);
          }
          return true;

        default:
          if (!s->multiline)
            return window_has_state(win, WINDOW_STATE_EDITING);
          return default_winproc(win, msg, wparam, lparam);
      }
    }

    // ── edSetText ───────────────────────────────────────────
    case edSetText: {
      if (!s || !lparam) return true;
      const char *src = (const char *)lparam;
      strncpy(s->buf, src, ME_BUF_SIZE - 1);
      s->buf[ME_BUF_SIZE - 1] = '\0';
      s->len      = (int)strlen(s->buf);
      if (s->multiline || s->cursor > s->len)
        s->cursor = s->len;
      s->scroll_y = 0;
      me_sync_window_fields(win, s);
      me_sync_scrollbar(win, s);
      invalidate_window(win);
      return true;
    }

    // ── edGetText ───────────────────────────────────────────
    case edGetText: {
      if (!s) return 0;
      if (!lparam || wparam == 0) return (lresult_t)s->len;
      int maxlen = (int)wparam;
      char *dst  = (char *)lparam;
      if (maxlen <= 0) return 0;
      strncpy(dst, s->buf, (size_t)(maxlen - 1));
      dst[maxlen - 1] = '\0';
      return (lresult_t)strlen(dst);
    }

    // ── evVScroll — scrollbar thumb dragged / arrow clicked ─────────────────
    case evVScroll: {
      if (!s) return true;
      int tw, th;
      me_text_dims(win, &tw, &th);
      int total_h   = calc_text_height(s->buf, tw);
      int max_scroll = total_h - th;
      if (max_scroll < 0) max_scroll = 0;
      s->scroll_y = (int)wparam;
      if (s->scroll_y > max_scroll) s->scroll_y = max_scroll;
      if (s->scroll_y < 0)         s->scroll_y = 0;
      me_sync_scrollbar(win, s);
      invalidate_window(win);
      return true;
    }

    // ── evResize — re-sync scrollbar after layout change ────────────────────
    case evResize:
      if (s) me_sync_scrollbar(win, s);
      return false;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}
