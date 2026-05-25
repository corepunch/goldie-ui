// ============================================================
// Command dispatcher for Form Editor menu actions.
// ============================================================

#include "formeditor.h"

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
  window_t *doc = g_app->active_form;

  switch (id) {
    case ID_FILE_NEW:
      create_form_doc(FORM_DEFAULT_W, FORM_DEFAULT_H);
      break;

    case ID_FILE_OPEN: {
      char path[512] = {0};
      window_t *owner = doc ? doc : g_app->windows[FE_WIN_MENUBAR];
      if (show_form_file_picker(owner, false, path, sizeof(path))) {
        if (!fe_project_load(path) && owner)
          message_box(owner, "Failed to load Orion project.", "Open", MB_OK);
      }
      break;
    }

    case ID_FILE_SAVE:
      if (g_app->project.loaded && g_app->project.filename[0]) {
        if (fe_project_save(g_app->project.filename)) {
          if (doc)
            send_message(doc, evStatusBar, 0, (void *)"Project saved");
        } else if (doc) {
          send_message(doc, evStatusBar, 0, (void *)"Project save failed");
        }
      } else {
        goto do_save_as;
      }
      break;

    do_save_as:
    case ID_FILE_SAVEAS: {
      if (!doc && g_app->form_count == 0) break;
      char path[512] = {0};
      window_t *owner = doc ? doc : g_app->windows[FE_WIN_MENUBAR];
      if (show_form_file_picker(owner, true, path, sizeof(path))) {
        if (fe_project_save(path)) {
          if (doc)
            send_message(doc, evStatusBar, 0, path);
        } else {
          if (doc)
            send_message(doc, evStatusBar, 0, (void *)"Project save failed");
        }
      }
      break;
    }

    case ID_FILE_QUIT:
#ifdef BUILD_AS_GEM
      if (g_app) {
        while (g_app->form_count > 0 && g_app->forms[0])
          close_form_doc(g_app->forms[0]);
        if (g_app->windows[FE_WIN_TOOL])    destroy_window(g_app->windows[FE_WIN_TOOL]);
        if (g_app->windows[FE_WIN_MENUBAR]) destroy_window(g_app->windows[FE_WIN_MENUBAR]);
      }
#else
      ui_request_quit();
#endif
      break;

    case ID_EDIT_DELETE: {
      if (doc)
        send_message(doc, evStatusBar, 0,
                     (void *)"Delete is disabled in window-only migration mode");
      break;
    }

    case ID_EDIT_PROPS: {
      if (!doc) break;
      window_t *owner = g_app->windows[FE_WIN_MENUBAR] ? g_app->windows[FE_WIN_MENUBAR] : doc;
      show_form_props_dialog(owner, doc);
      break;
    }
  }
}

bool fe_controller_drop_create(window_t *doc,
                               const ui_drag_item_payload_t *payload,
                               window_t *target) {
  if (!doc || !payload)
    return false;

  if (payload->item_type != UI_DRAG_ITEM_CONTROL_CLASS)
    return false;

  int component_id = (int)payload->item_class;
  if (component_id < 0)
    return false;
  if (!target)
    return false;

  const fe_component_desc_t *desc = fe_component_at(component_id);
  if (!desc)
    return false;
  if (!send_message(target, evAcceptsDrop,
                    MAKEDWORD(UI_DRAG_ITEM_CONTROL_CLASS, (uint16_t)component_id),
                    (void *)payload)) {
    return false;
  }
  if (fe_component_rejects_parent(desc, target))
    return false;

  if (!fe_doc_drop_create_component(component_id, target))
    return false;

  if (doc->children)
    invalidate_window(doc->children);
  property_browser_refresh(doc);
  forms_browser_refresh();
  return true;
}
