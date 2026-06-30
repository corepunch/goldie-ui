// Modal dialog implementation (WinAPI-style show_dialog / end_dialog).
typedef struct {
// This file moved to commdlg/dialog.c
  (void)send_message(ctrl, cbGetCurrentSelection, 0, &v);
  *(int *)(base + b->offset) = (v >= 0) ? v : (int)b->wparam;
}

void ddx_push_check(window_t *dlg, const ctrl_binding_t *b, const void *state) {
  const char *base = (const char *)state;
  window_t *ctrl = get_window_item(dlg, b->ctrl_id);
  bool v;
  if (!ctrl) return;
  v = *(const bool *)(base + b->offset);
  send_message(ctrl, btnSetCheck, v ? btnStateChecked : btnStateUnchecked, NULL);
}

void ddx_pull_check(window_t *dlg, const ctrl_binding_t *b, void *state) {
  char *base = (char *)state;
  window_t *ctrl = get_window_item(dlg, b->ctrl_id);
  int v;
  if (!ctrl) return;
  v = (int)send_message(ctrl, btnGetCheck, 0, NULL);
  *(bool *)(base + b->offset) = (v == btnStateChecked);
}

void dialog_push(window_t *win, const void *state,
                 const ctrl_binding_t *b, int n) {
  if (!win || !state || !b) return;
  const char *base = (const char *)state;
  for (int i = 0; i < n; i++) {
    if (b[i].push) {
      b[i].push(win, &b[i], state);
      continue;
    }

    window_t *ctrl = get_window_item(win, b[i].ctrl_id);
    if (!ctrl) continue;
    switch (b[i].getter) {
      case 0:
        break;
      case cbGetCurrentSelection: {
        int v = *(const int *)(base + b[i].offset);
        if (v < 0) v = (int)b[i].wparam;
        send_message(ctrl, cbSetCurrentSelection, (uint32_t)v, NULL);
        break;
      }
      case edGetText:
        send_message(ctrl, edSetText, 0, (void *)(base + b[i].offset));
        break;
      default:
        break;
    }
  }
}

int dialog_pull_command(window_t *win, void *state,
                        const ctrl_binding_t *b, int n,
                        uint16_t command) {
  if (!win || !state || !b) return 0;
  char *base = (char *)state;
  int applied = 0;
  for (int i = 0; i < n; i++) {
    if (command && b[i].command && b[i].command != command)
      continue;

    if (b[i].pull) {
      b[i].pull(win, &b[i], state);
      applied++;
      continue;
    }

    window_t *ctrl = get_window_item(win, b[i].ctrl_id);
    if (!ctrl) continue;
    switch (b[i].getter) {
      case 0:
        applied++;
        break;
      case cbGetCurrentSelection: {
        int v = kComboBoxError;
        (void)send_message(ctrl, cbGetCurrentSelection, 0, &v);
        *(int *)(base + b[i].offset) = (v >= 0) ? v : (int)b[i].wparam;
        applied++;
        break;
      }
      case edGetText: {
        char  *dst = base + b[i].offset;
        size_t sz  = b[i].wparam;
        if (sz > 0)
          send_message(ctrl, edGetText, (uint32_t)sz, dst);
        applied++;
        break;
      }
      default:
        (void)send_message(ctrl, b[i].getter, (uint32_t)b[i].wparam,
                           base + b[i].offset);
        applied++;
        break;
    }
  }
  return applied;
}

void dialog_pull(window_t *win, void *state,
                 const ctrl_binding_t *b, int n) {
  (void)dialog_pull_command(win, state, b, n, 0);
}

// ── Database-Driven Dialogs ────────────────────────────────────────────────
// Extension of DDX system that works directly with database records.
// Fetch record on open, push to controls, pull on OK, save to database.

typedef struct {
  form_def_t const *def;          // form definition with db_table metadata
  database_t       *db;           // database instance
  int               record_id;    // record ID (0 = INSERT, >0 = UPDATE)
  void             *record_buf;   // allocated buffer for record struct
  bool              is_new;       // true if creating new record
  const char       *fk_field;     // FK field name (e.g., "post_id"), optional
  int               fk_value;     // FK value to set for new records
} db_dlg_ctx_t;

static result_t dialog_db_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  db_dlg_ctx_t *ctx = (db_dlg_ctx_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      ctx = (db_dlg_ctx_t *)lparam;
      win->userdata = ctx;
      
      if (!ctx->def->db_table || !ctx->def->db_fields || !ctx->db) {
        // Missing database metadata - can't proceed
        end_dialog(win, 0);
        return true;
      }
      
      // Fetch record from database if updating (record_id > 0)
      if (ctx->record_id > 0) {
        lresult_t result = send_db_message(ctx->db, dbFind,
          MAKEDWORD(ctx->def->db_table_id, 0),
          (void *)(intptr_t)ctx->record_id);
        void *fetched = (void *)result;
        
        if (fetched) {
          // Copy fetched record to our buffer
          // TODO: Calculate exact struct size from field metadata
          size_t struct_size = 512; // Reasonable default for most records
          memcpy(ctx->record_buf, fetched, struct_size);
          ctx->is_new = false;
        } else {
          // Record not found - treat as new
          ctx->is_new = true;
        }
      } else {
        // New record (INSERT mode)
        ctx->is_new = true;
        
        // Set FK field if provided (e.g., post_id for comments)
        if (ctx->fk_field && ctx->fk_value > 0 && ctx->def->db_fields) {
          const db_field_meta_t *fields = (const db_field_meta_t *)ctx->def->db_fields;
          // Find field offset by name
          for (int i = 0; i < ctx->def->db_field_count; i++) {
            if (strcmp(fields[i].name, ctx->fk_field) == 0) {
              // Set FK value at field offset
              int *fk_ptr = (int *)((char *)ctx->record_buf + fields[i].offset);
              *fk_ptr = ctx->fk_value;
              break;
            }
          }
        }
      }
      
      // Push record fields to controls using DDX
      if (ctx->def->bindings && ctx->def->binding_count > 0) {
        dialog_push(win, ctx->record_buf,
                    ctx->def->bindings, ctx->def->binding_count);
      }
      return true;
    }

    case evCommand: {
      if (!ctx) return false;
      uint16_t notif = HIWORD(wparam);

      // Pressing Enter in edit box - save and close
      if (notif == edUpdate) {
        if (ctx->def->bindings) {
          dialog_pull(win, ctx->record_buf,
                      ctx->def->bindings, ctx->def->binding_count);
        }
        
        // Save to database
        if (ctx->is_new) {
          // INSERT
          send_db_message(ctx->db, dbInsert,
            ctx->def->db_table_id, ctx->record_buf);
        } else {
          // UPDATE
          send_db_message(ctx->db, dbUpdate,
            MAKEDWORD(ctx->def->db_table_id, ctx->record_id),
            ctx->record_buf);
        }
        
        end_dialog(win, 1);
        return true;
      }

      if (notif != btnClicked) return false;
      window_t *src = (window_t *)lparam;
      if (!src) return false;

      if (ctx->def->ok_id && src->id == ctx->def->ok_id) {
        // Pull controls → record buffer
        if (ctx->def->bindings) {
          dialog_pull(win, ctx->record_buf,
                      ctx->def->bindings, ctx->def->binding_count);
        }
        
        // Save to database
        if (ctx->is_new) {
          // INSERT
          send_db_message(ctx->db, dbInsert,
            ctx->def->db_table_id, ctx->record_buf);
        } else {
          // UPDATE  
          send_db_message(ctx->db, dbUpdate,
            MAKEDWORD(ctx->def->db_table_id, ctx->record_id),
            ctx->record_buf);
        }
        
        end_dialog(win, 1);
        return true;
      }
      
      if (ctx->def->cancel_id && src->id == ctx->def->cancel_id) {
        end_dialog(win, 0);
        return true;
      }
      return false;
    }

    default:
      return false;
  }
}

