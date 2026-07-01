// Terminal — Quake-style command console.
// Reads keyboard input and dispatches commands from a built-in table.

#include "vgat.h"
#include <unistd.h>
#include <time.h>
#include <dirent.h>

// ── Lua extraspace access (stores vgat_state_t*) ──────────────────────────
#if defined(HAVE_LUA)
#define VGATSTATE(L) ((vgat_state_t**)lua_getextraspace(L))

static int f_print(lua_State *L) {
  vgat_state_t *st = *VGATSTATE(L);
  for (int i = 1, n = lua_gettop(L); i <= n; i++) {
    vgat_screen_write_string(&st->screen, lua_tostring(L, i), VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    if (i < n) vgat_screen_write_string(&st->screen, "\t", VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  }
  vgat_screen_newline(&st->screen);
  return 0;
}

static int f_io_read(lua_State *L) { return lua_yield(L, 0); }

static int f_io_write(lua_State *L) {
  vgat_state_t *st = *VGATSTATE(L);
  for (int i = 1, n = lua_gettop(L); i <= n; i++) {
    const char *s = luaL_checkstring(L, i);
    vgat_screen_write_string(&st->screen, s, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    fprintf(stdout, "%s", s);
  }
  return 0;
}

static int f_stdout_write(lua_State *L) {
  vgat_state_t *st = *VGATSTATE(L);
  for (int i = 2, n = lua_gettop(L); i <= n; i++) {
    const char *s = luaL_checkstring(L, i);
    vgat_screen_write_string(&st->screen, s, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    fprintf(stdout, "%s", s);
  }
  lua_pushvalue(L, 1);
  return 1;
}

static int f_stdout_flush(lua_State *L) { lua_pushvalue(L, 1); return 1; }
static int f_stdout_setvbuf(lua_State *L) { lua_pushvalue(L, 1); return 1; }

static const char *STDOUT_METATABLE = "terminal.stdout";

static bool create_lua_state(vgat_state_t *st) {
  st->L = luaL_newstate();
  if (!st->L) return false;
  luaL_openlibs(st->L);

  *(vgat_state_t **)lua_getextraspace(st->L) = st;

  // Override print
  lua_pushcfunction(st->L, f_print);
  lua_setglobal(st->L, "print");

  // Setup stdout metatable
  luaL_newmetatable(st->L, STDOUT_METATABLE);
  lua_pushvalue(st->L, -1);                   lua_setfield(st->L, -2, "__index");
  lua_pushcfunction(st->L, f_stdout_write);   lua_setfield(st->L, -2, "write");
  lua_pushcfunction(st->L, f_stdout_flush);   lua_setfield(st->L, -2, "flush");
  lua_pushcfunction(st->L, f_stdout_setvbuf); lua_setfield(st->L, -2, "setvbuf");
  lua_pop(st->L, 1);

  lua_newuserdata(st->L, sizeof(void *));
  luaL_setmetatable(st->L, STDOUT_METATABLE);

  // Override io.*
  lua_getglobal(st->L, "io");
  lua_pushvalue(st->L, -2);             lua_setfield(st->L, -2, "output");
  lua_pushvalue(st->L, -2);             lua_setfield(st->L, -2, "stdout");
  lua_pushcfunction(st->L, f_io_write); lua_setfield(st->L, -2, "write");
  lua_pushcfunction(st->L, f_io_read);  lua_setfield(st->L, -2, "read");
  lua_pop(st->L, 2);

  return true;
}

static void continue_lua_coroutine(vgat_state_t *st, int nargs) {
  int nres;
  int status = lua_resume(st->co, NULL, nargs, &nres);

  if (status == LUA_OK) {
    vgat_screen_write_string(&st->screen, "\nProcess finished\n", 10, VGAT_BG_DEFAULT);
    st->lua_running = false;
    st->waiting_for_input = false;
  } else if (status == LUA_YIELD) {
    st->waiting_for_input = true;
    vgat_screen_write_string(&st->screen, "\n> ", VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  } else {
    vgat_screen_write_string(&st->screen, "\nError: ", 9, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, lua_tostring(st->co, -1), VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_newline(&st->screen);
    st->lua_running = false;
    st->waiting_for_input = false;
  }
}
#endif /* HAVE_LUA */

// ── Command forward declarations ──────────────────────────────────────────

static void cmd_echo(vgat_state_t *, int, char **);
static void cmd_help(vgat_state_t *, int, char **);
static void cmd_clear(vgat_state_t *, int, char **);
static void cmd_dir(vgat_state_t *, int, char **);
static void cmd_pwd (vgat_state_t *, int, char **);
static void cmd_cd  (vgat_state_t *, int, char **);
static void cmd_cat (vgat_state_t *, int, char **);
static void cmd_date(vgat_state_t *, int, char **);
static void cmd_whoami(vgat_state_t *, int, char **);
static void cmd_lua  (vgat_state_t *, int, char **);

// ── Command table ─────────────────────────────────────────────────────────

const vgat_cmd_t g_cmds[] = {
  {"echo",   "Echo text",              cmd_echo},
  {"help",   "Show available commands",cmd_help},
  {"clear",  "Clear the console",      cmd_clear},
  {"dir",    "List directory contents", cmd_dir},
  {"ls",     "List directory contents", cmd_dir},
  {"pwd",    "Print working directory", cmd_pwd},
  {"cd",     "Change directory",       cmd_cd},
  {"cat",    "Display a file",         cmd_cat},
  {"date",   "Show date and time",     cmd_date},
  {"whoami", "Show current user",      cmd_whoami},
  {"lua",    "Run a Lua script",        cmd_lua},
  {NULL, NULL, NULL} // sentinel
};

// ── Command implementations ───────────────────────────────────────────────

static void cmd_echo(vgat_state_t *st, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (i > 1) vgat_screen_write_string(&st->screen, " ", VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, argv[i], VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  }
  vgat_screen_newline(&st->screen);
}

static void cmd_help(vgat_state_t *st, int argc, char **argv) {
  (void)argc; (void)argv;
  vgat_screen_write_string(&st->screen, "Available commands:\n", 10, VGAT_BG_DEFAULT);
  for (int i = 0; g_cmds[i].name; i++) {
    char line[128];
    int n = snprintf(line, sizeof(line), "  %-12s %s\n", g_cmds[i].name, g_cmds[i].help);
    if (n > 0) vgat_screen_write_string(&st->screen, line, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  }
}

static void cmd_clear(vgat_state_t *st, int argc, char **argv) {
  (void)argc; (void)argv;
  vgat_screen_clear(&st->screen);
}

static void cmd_dir(vgat_state_t *st, int argc, char **argv) {
  (void)argc;
  const char *path = argv[1] ? argv[1] : ".";
  DIR *d = opendir(path);
  if (!d) {
    vgat_screen_write_string(&st->screen, "Cannot open directory: ", 9, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, path, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_newline(&st->screen);
    return;
  }
  struct dirent *e;
  while ((e = readdir(d))) {
    vgat_screen_write_string(&st->screen, e->d_name, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, "  ", VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  }
  closedir(d);
  vgat_screen_newline(&st->screen);
}

static void cmd_pwd(vgat_state_t *st, int argc, char **argv) {
  (void)argc; (void)argv;
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd))) {
    vgat_screen_write_string(&st->screen, cwd, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_newline(&st->screen);
  }
}

static void cmd_cd(vgat_state_t *st, int argc, char **argv) {
  (void)argc;
  const char *path = argv[1] ? argv[1] : getenv("HOME");
  if (!path) path = "/";
  if (chdir(path) != 0) {
    vgat_screen_write_string(&st->screen, "cd: ", 9, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, path, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, ": No such directory\n", 9, VGAT_BG_DEFAULT);
  }
}

static void cmd_cat(vgat_state_t *st, int argc, char **argv) {
  if (argc < 2) {
    vgat_screen_write_string(&st->screen, "Usage: cat <file>\n", 9, VGAT_BG_DEFAULT);
    return;
  }
  FILE *fp = fopen(argv[1], "r");
  if (!fp) {
    vgat_screen_write_string(&st->screen, "cat: ", 9, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, argv[1], VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, ": No such file\n", 9, VGAT_BG_DEFAULT);
    return;
  }
  char buf[256];
  while (fgets(buf, sizeof(buf), fp)) {
    vgat_screen_write_string(&st->screen, buf, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  }
  fclose(fp);
}

static void cmd_date(vgat_state_t *st, int argc, char **argv) {
  (void)argc; (void)argv;
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char buf[64];
  if (strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Z %Y", tm)) {
    vgat_screen_write_string(&st->screen, buf, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_newline(&st->screen);
  }
}

static void cmd_whoami(vgat_state_t *st, int argc, char **argv) {
  (void)argc; (void)argv;
  const char *user = getenv("USER");
  if (!user) user = getenv("USERNAME");
  if (!user) user = "unknown";
  vgat_screen_write_string(&st->screen, user, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  vgat_screen_newline(&st->screen);
}

static void cmd_lua(vgat_state_t *st, int argc, char **argv) {
#if defined(HAVE_LUA)
  if (argc < 2) {
    vgat_screen_write_string(&st->screen, "Usage: lua <script.lua>\n", 9, VGAT_BG_DEFAULT);
    return;
  }

  // Create Lua state on first use
  if (!st->L) {
    if (!create_lua_state(st)) {
      vgat_screen_write_string(&st->screen, "Error: Failed to create Lua state\n", 9, VGAT_BG_DEFAULT);
      return;
    }
  }

  // Create a coroutine and load the script
  st->co = lua_newthread(st->L);
  st->lua_running = true;
  st->waiting_for_input = false;

  if (luaL_loadfile(st->co, argv[1]) != LUA_OK) {
    vgat_screen_write_string(&st->screen, "Error loading file: ", 9, VGAT_BG_DEFAULT);
    vgat_screen_write_string(&st->screen, lua_tostring(st->co, -1), VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
    vgat_screen_newline(&st->screen);
    st->lua_running = false;
    return;
  }

  continue_lua_coroutine(st, 0);
#else
  (void)st; (void)argc; (void)argv;
  vgat_screen_write_string(&st->screen, "Lua scripting is not available in this build.\n",
                           9, VGAT_BG_DEFAULT);
#endif
}

// ── Input processing ──────────────────────────────────────────────────────

static void process_input(vgat_state_t *st) {
  // Echo command to scrollback
  vgat_screen_write_string(&st->screen, "> ", 10, VGAT_BG_DEFAULT);
  vgat_screen_write_string(&st->screen, st->input_buf, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  vgat_screen_newline(&st->screen);

  // Trim leading whitespace
  char *p = st->input_buf;
  while (*p == ' ' || *p == '\t') p++;
  if (!*p) return;

  // Parse argv (max 64 args, modifies string in-place)
  int argc = 0;
  char *argv[64];
  bool in = false;
  for (char *q = p; *q && argc < 64; q++) {
    if (*q == ' ' || *q == '\t') {
      *q = '\0';
      in = false;
    } else if (!in) {
      argv[argc++] = q;
      in = true;
    }
  }

  // Dispatch
  for (int i = 0; g_cmds[i].name; i++) {
    if (strcmp(argv[0], g_cmds[i].name) == 0) {
      g_cmds[i].func(st, argc, argv);
      return;
    }
  }

  vgat_screen_write_string(&st->screen, "Unknown command: ", 9, VGAT_BG_DEFAULT);
  vgat_screen_write_string(&st->screen, argv[0], VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
  vgat_screen_newline(&st->screen);
}

// ── Window procedure ──────────────────────────────────────────────────────

result_t terminal_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  vgat_state_t *st = (vgat_state_t *)win->userdata;

  switch (msg) {

    case evCreate: {
      st = (vgat_state_t *)calloc(1, sizeof(vgat_state_t));
      win->userdata = st;
      if (!st) return false;
      st->win = win;
      st->default_fg = kAnsi16[VGAT_FG_DEFAULT];
      st->default_bg = kAnsi16[VGAT_BG_DEFAULT];
      st->scroll_pos = 0;
      st->cursor_visible = true;
      st->cursor_blink_ctr = 0;

      char font_path[512];
      snprintf(font_path, sizeof(font_path), "%s/../share/orion/fonts/monoid.ttf",
               ui_get_exe_dir());
      vga_font_init(font_path, 12.0f);

      irect16_t cr = get_client_rect(win);
      int cols = cr.w / vga_char_width();
      int rows = cr.h / vga_char_height();
      if (cols <= 0) cols = VGAT_DEFAULT_COLS;
      if (rows <= 0) rows = VGAT_DEFAULT_ROWS;

      vgat_screen_init(&st->screen, rows, cols);

      // Welcome message
      vgat_screen_write_string(&st->screen, "Terminal v1.0\n", 10, VGAT_BG_DEFAULT);
      vgat_screen_write_string(&st->screen, "Type 'help' for available commands\n", 10, VGAT_BG_DEFAULT);
      vgat_screen_newline(&st->screen);

      st->timer_id = axSetTimer(win, VGAT_TIMER_INTERVAL_MS, NULL, true);
      return true;
    }

    case evDestroy: {
      if (st) {
        if (st->timer_id > 0) axCancelTimer(st->timer_id);
        vga_font_shutdown();
        vgat_screen_shutdown(&st->screen);
#if defined(HAVE_LUA)
        if (st->L) lua_close(st->L);
#endif
        free(st);
        win->userdata = NULL;
      }
      return false;
    }

    case evTimer: {
      if (!st) return true;
      st->cursor_blink_ctr++;
      if (st->cursor_blink_ctr >= (VGAT_CURSOR_BLINK_MS / VGAT_TIMER_INTERVAL_MS)) {
        st->cursor_blink_ctr = 0;
        st->cursor_visible = !st->cursor_visible;
        invalidate_window(win);
      }
      return true;
    }

    case evPaint: {
      if (!st || !st->screen.rows) return true;

      int cw = vga_char_width();
      int ch = vga_char_height();
      irect16_t cr = get_client_rect(win);
      int vis_cols = cr.w / cw;
      int vis_rows = cr.h / ch;
      if (vis_cols <= 0 || vis_rows <= 0) return true;

      vga_text_grid_t grid;
      if (!vga_text_ensure_grid(&grid, vis_cols, vis_rows)) return true;
      vga_text_clear_grid(&grid, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);

      // ── Scrollback output area (top vis_rows-1) ──
      int content_rows = vis_rows - 1;
      if (content_rows > 0) {
        int written = st->screen.cursor_row;
        int first = written - content_rows - st->scroll_pos;
        if (first < 0) first = 0;

        for (int row = 0; row < content_rows; row++) {
          int logical = first + row;
          if (logical >= written) break;
          int phys = (st->screen.head + logical) % st->screen.total_rows;
          for (int col = 0; col < vis_cols; col++) {
            if (col >= st->screen.cols) break;
            vgat_cell *cell = &st->screen.rows[phys * st->screen.cols + col];
            vga_text_set_cell(&grid, col, row, cell->ch, cell->fg, cell->bg);
          }
        }
      }

      // ── Input line (last row) ──
      int input_row = vis_rows - 1;
      int col = 0;
      const char *prompt = "> ";
      for (const char *cp = prompt; *cp && col < vis_cols; cp++, col++)
        vga_text_set_cell(&grid, col, input_row, *cp, 10, VGAT_BG_DEFAULT);

      for (int i = 0; i < st->input_len && col < vis_cols; i++, col++)
        vga_text_set_cell(&grid, col, input_row, st->input_buf[i], VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);

      if (st->cursor_visible && col < vis_cols)
        vga_text_set_cell(&grid, col, input_row, ' ', VGAT_BG_DEFAULT, VGAT_FG_DEFAULT);

      // ── Render ──
      if (R_UpdateTextureRG8(grid.cells_tex, 0, 0, grid.cells_w, grid.cells_h, grid.cells)) {
        R_VgaBuffer buf = {
          .vga_buffer = grid.cells_tex,
          .width = grid.cells_w,
          .height = grid.cells_h,
        };
        VgaFontLayout fl = vga_get_font_layout();
        R_DrawVGABuffer(&buf, cr.x, cr.y,
                        grid.cells_w * cw,
                        grid.cells_h * ch,
                        (const R_FontSheet*)&fl, kAnsi16);
      }

      vga_text_free_grid(&grid);
      return true;
    }

    case evResize: {
      if (!st) return false;
      irect16_t cr = get_client_rect(win);
      int cols = cr.w / vga_char_width();
      int rows = cr.h / vga_char_height();
      if (cols <= 0 || rows <= 0) return false;

      vgat_screen_resize(&st->screen, cols);

      int content_rows = rows - 1;
      int written = st->screen.cursor_row;
      int max_scroll = written > content_rows ? written - content_rows : 0;
      scroll_info_t si = {
        .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
        .nMin = 0,
        .nMax = max_scroll,
        .nPage = (uint32_t)content_rows,
        .nPos = st->scroll_pos,
      };
      set_scroll_info(win, SB_VERT, &si, true);
      invalidate_window(win);
      return false;
    }

    case evVScroll: {
      if (!st) return false;
      irect16_t cr = get_client_rect(win);
      int vis_rows = cr.h / vga_char_height();
      if (vis_rows <= 0) vis_rows = 24;
      int content_rows = vis_rows - 1;
      int written = st->screen.cursor_row;
      int max_scroll = written > content_rows ? written - content_rows : 0;
      int new_pos = CLAMP((int)wparam, 0, max_scroll);
      if (new_pos != st->scroll_pos) {
        st->scroll_pos = new_pos;
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_pos };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
      }
      return true;
    }

    case evWheel: {
      if (!st) return false;
      int delta = (int16_t)HIWORD((uintptr_t)lparam);
      irect16_t cr = get_client_rect(win);
      int vis_rows = cr.h / vga_char_height();
      if (vis_rows <= 0) vis_rows = 24;
      int content_rows = vis_rows - 1;
      int written = st->screen.cursor_row;
      int max_scroll = written > content_rows ? written - content_rows : 0;
      int lines = delta < 0 ? 3 : -3;
      int new_pos = CLAMP(st->scroll_pos + lines, 0, max_scroll);
      if (new_pos != st->scroll_pos) {
        st->scroll_pos = new_pos;
        scroll_info_t si = { .fMask = SIF_POS, .nPos = st->scroll_pos };
        set_scroll_info(win, SB_VERT, &si, false);
        invalidate_window(win);
      }
      return true;
    }

    case evKeyDown: {
      if (!st) return false;
      switch (wparam) {
        case AX_KEY_ENTER:
          if (st->lua_running && st->waiting_for_input) {
            vgat_screen_write_string(&st->screen, "> ", 10, VGAT_BG_DEFAULT);
            vgat_screen_write_string(&st->screen, st->input_buf, VGAT_FG_DEFAULT, VGAT_BG_DEFAULT);
            vgat_screen_newline(&st->screen);
#if defined(HAVE_LUA)
            lua_pushstring(st->co, st->input_buf);
            continue_lua_coroutine(st, 1);
#endif
          } else {
            process_input(st);
          }
          st->input_len = 0;
          st->input_buf[0] = '\0';
          invalidate_window(win);
          return true;
        case AX_KEY_BACKSPACE:
          if (st->input_len > 0) {
            st->input_buf[--st->input_len] = '\0';
            invalidate_window(win);
          }
          return true;
        default:
          return false;
      }
    }

    case evTextInput: {
      if (!st) return false;
      const char *text = (const char *)lparam;
      while (*text && st->input_len < VGAT_INPUT_MAX - 1) {
        if (*text >= 0x20 && *text < 0x7F) {
          st->input_buf[st->input_len++] = *text;
        }
        text++;
      }
      st->input_buf[st->input_len] = '\0';
      invalidate_window(win);
      return true;
    }

    default:
      return 0;
  }
}
