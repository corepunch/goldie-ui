// ============================================================
// Command dispatcher for Form Editor menu actions.
// ============================================================

#include "formeditor.h"
#include "fe_commands.h"

// ============================================================
// File picker helper
// ============================================================

static bool show_form_file_picker(window_t *parent, bool save_mode,
                                   char *out_path, size_t out_sz) {
  openfilename_t ofn = {0};
  ofn.lStructSize  = sizeof(ofn);
  ofn.hwndOwner    = parent;
  ofn.lpstrFile    = out_path;
  ofn.nMaxFile     = (uint32_t)out_sz;
  ofn.lpstrFilter  = "Orion Projects\0*.orion\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags        = save_mode ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST;
  return save_mode ? get_save_filename(&ofn) : get_open_filename(&ofn);
}

// ============================================================
// Menu command handler
// ============================================================

void handle_menu_command(uint16_t id) {
  if (!g_app) return;
  form_doc_t *doc = g_app->doc;

  switch (id) {
    case ID_FILE_NEW:
      create_form_doc(FORM_DEFAULT_W, FORM_DEFAULT_H);
      break;

    case ID_FILE_OPEN: {
      char path[512] = {0};
      window_t *owner = doc ? doc->doc_win : (g_app->menubar_win);
      if (show_form_file_picker(owner, false, path, sizeof(path))) {
        if (!fe_project_load(path) && owner)
          message_box(owner, "Failed to load Orion project.", "Open", MB_OK);
      }
      break;
    }

    case ID_FILE_SAVE:
      if (g_app->project.loaded && g_app->project.filename[0]) {
        if (fe_project_save(g_app->project.filename)) {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, (void *)"Project saved");
        } else if (doc && doc->doc_win) {
          send_message(doc->doc_win, evStatusBar, 0, (void *)"Project save failed");
        }
      } else {
        goto do_save_as;
      }
      break;

    do_save_as:
    case ID_FILE_SAVEAS: {
      if (!doc && !g_app->docs) break;
      char path[512] = {0};
      window_t *owner = doc ? doc->doc_win : g_app->menubar_win;
      if (show_form_file_picker(owner, true, path, sizeof(path))) {
        if (fe_project_save(path)) {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, path);
        } else {
          if (doc && doc->doc_win)
            send_message(doc->doc_win, evStatusBar, 0, (void *)"Project save failed");
        }
      }
      break;
    }

    case ID_FILE_QUIT:
#ifdef BUILD_AS_GEM
      if (g_app) {
        while (g_app->docs)
          close_form_doc(g_app->docs);
        if (g_app->tool_win)    destroy_window(g_app->tool_win);
        if (g_app->menubar_win) destroy_window(g_app->menubar_win);
      }
#else
      ui_request_quit();
#endif
      break;

    case ID_EDIT_DELETE: {
      if (!doc) break;
      window_t *cwin = doc->canvas_win;
      if (!cwin) break;
      canvas_state_t *cs = (canvas_state_t *)cwin->userdata;
      if (!cs || cs->selected_idx < 0) break;
      int idx = cs->selected_idx;
      if (doc->elements[idx].live_win)
        destroy_window(doc->elements[idx].live_win);
      // Remove element by shifting the array
      for (int i = idx; i < doc->element_count - 1; i++)
        doc->elements[i] = doc->elements[i + 1];
      doc->element_count--;
      cs->selected_idx = -1;
      fe_doc_mark_modified(doc);
      canvas_rebuild_live_controls(doc);
      break;
    }

    case ID_EDIT_PROPS: {
      if (!doc) break;
      window_t *cwin = doc->canvas_win;
      if (!cwin) break;
      canvas_state_t *cs = (canvas_state_t *)cwin->userdata;
      window_t *owner = g_app->menubar_win ? g_app->menubar_win : doc->doc_win;
      if (!cs || cs->selected_idx < 0) {
        show_form_props_dialog(owner, doc);
      } else {
        form_element_t *el = &doc->elements[cs->selected_idx];
        if (show_props_dialog(owner, el)) {
          fe_doc_mark_modified(doc);
          property_browser_refresh(doc);
        }
      }
      break;
    }
  }
}
