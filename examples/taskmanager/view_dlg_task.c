// VIEW: Task create/edit dialog using form-based layout.

#include "taskmanager.h"

// ============================================================
// Form layout — auto-layout: top-level vertical stack,
// field rows in a 2-column grid, button row horizontal.
// ============================================================

static const form_ctrl_def_t kTaskFieldLabels[] = {
  { .class_name = "Label",    .text = "Title:",        .name = "lbl_title",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "Label",     .text = "Description:", .name = "lbl_desc",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_START },
  { .class_name = "Label",    .text = "Priority:",     .name = "lbl_prio",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "Label",    .text = "Status:",       .name = "lbl_status",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "Label",    .text = "Due (epoch):",  .name = "lbl_due",
    .h_align = LAYOUT_ALIGN_START, .v_align = LAYOUT_ALIGN_CENTER },
};

static const form_ctrl_def_t kTaskFieldInputs[] = {
  { .class_name = "TextBox", .id = ID_TASK_TITLE_CTRL, .name = "edit_title",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "MultiEdit", .id = ID_TASK_DESC_CTRL, .name = "edit_desc",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_STRETCH },
  { .class_name = "ComboBox", .id = ID_TASK_PRIORITY_CTRL, .name = "combo_prio",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "ComboBox", .id = ID_TASK_STATUS_CTRL, .name = "combo_status",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
  { .class_name = "TextBox", .id = ID_TASK_DUEDATE_CTRL, .name = "edit_due",
    .h_align = LAYOUT_ALIGN_STRETCH, .v_align = LAYOUT_ALIGN_CENTER },
};

static const form_ctrl_def_t kTaskBtnRow[] = {
  { .class_name = "Button", .id = ID_OK,     .flags = BUTTON_DEFAULT, .text = "OK",
    .name = "btn_ok",     .h_align = LAYOUT_ALIGN_START },
  { .class_name = "Button", .id = ID_CANCEL, .text = "Cancel",
    .name = "btn_cancel", .h_align = LAYOUT_ALIGN_START },
};

static const form_ctrl_def_t kTaskEditChildren[] = {
  {
    .class_name         = "GridView",
    .name               = "fields",
    .flags              = WINDOW_FLEXSPACE,
    .layout_spacing     = 4,
    .h_align            = LAYOUT_ALIGN_STRETCH,
    .v_align            = LAYOUT_ALIGN_STRETCH,
    .children           = (const form_ctrl_def_t[]){
      {
        .class_name = "Column",
        .name = "labels",
        .size = {120, 0},
        .children = kTaskFieldLabels,
        .child_count = (int)(sizeof(kTaskFieldLabels)/sizeof(kTaskFieldLabels[0])),
      },
      {
        .class_name = "Column",
        .name = "inputs",
        .flags = WINDOW_FLEXSPACE,
        .children = kTaskFieldInputs,
        .child_count = (int)(sizeof(kTaskFieldInputs)/sizeof(kTaskFieldInputs[0])),
      },
    },
    .child_count        = 2,
  },
  {
    .class_name         = "StackView",
    .name               = "actions",
    .flags = WINDOW_STACK_HORIZONTAL,
    .layout_spacing     = 6,
    .h_align            = LAYOUT_ALIGN_CENTER,
    .v_align            = LAYOUT_ALIGN_START,
    .children           = kTaskBtnRow,
    .child_count        = (int)(sizeof(kTaskBtnRow)/sizeof(kTaskBtnRow[0])),
  },
};

// ============================================================
// Dialog state
// ============================================================

typedef struct {
  task_t *task;       // NULL = create new, non-NULL = edit existing
  bool    accepted;
  char    title[128];
  char    desc[512];
  int     priority;
  int     status;
  uint32_t due_date;
} task_dlg_state_t;

// ============================================================
// DDX binding table
// ============================================================

static const ctrl_binding_t k_task_bindings[] = {
  DDX_TEXT(ID_TASK_TITLE_CTRL, task_dlg_state_t, title),
  DDX_TEXT(ID_TASK_DESC_CTRL, task_dlg_state_t, desc),
  DDX_COMBO(ID_TASK_PRIORITY_CTRL, task_dlg_state_t, priority, PRIORITY_NORMAL),
  DDX_COMBO(ID_TASK_STATUS_CTRL, task_dlg_state_t, status, STATUS_TODO),
};

static const form_def_t kTaskEditForm = {
  .name           = "Task",
  .width          = 300,
  .height         = 220,
  .flags = (0) | WINDOW_AUTO_LAYOUT,
  .layout_spacing = 6,
  .padding        = {8, 8, 8, 8},
  .children       = kTaskEditChildren,
  .child_count    = (int)(sizeof(kTaskEditChildren)/sizeof(kTaskEditChildren[0])),
  .bindings       = k_task_bindings,
  .binding_count  = ARRAY_LEN(k_task_bindings),
  .ok_id          = ID_OK,
  .cancel_id      = ID_CANCEL,
};

// ============================================================
// Helpers to initialise combo boxes
// ============================================================

static void populate_priority_combo(window_t *win) {
  window_t *cb = get_window_item(win, ID_TASK_PRIORITY_CTRL);
  if (!cb) return;
  send_message(cb, cbAddString, 0, (void *)"Low");
  send_message(cb, cbAddString, 0, (void *)"Normal");
  send_message(cb, cbAddString, 0, (void *)"High");
  send_message(cb, cbAddString, 0, (void *)"Urgent");
}

static void populate_status_combo(window_t *win) {
  window_t *cb = get_window_item(win, ID_TASK_STATUS_CTRL);
  if (!cb) return;
  send_message(cb, cbAddString, 0, (void *)"Todo");
  send_message(cb, cbAddString, 0, (void *)"In Progress");
  send_message(cb, cbAddString, 0, (void *)"Completed");
  send_message(cb, cbAddString, 0, (void *)"Cancelled");
}

// ============================================================
// Window procedure
// ============================================================

static lresult_t task_dlg_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  task_dlg_state_t *s = (task_dlg_state_t *)win->userdata;

  switch (msg) {
    case evCreate: {
      win->userdata = lparam;
      s = (task_dlg_state_t *)lparam;

      // Populate combo items first (must precede dialog_push).
      populate_priority_combo(win);
      populate_status_combo(win);

      if (s->task) {
        // Copy existing task data into state so dialog_push can populate controls.
        strncpy(s->title, s->task->title, sizeof(s->title) - 1);
        s->title[sizeof(s->title) - 1] = '\0';
        strncpy(s->desc, s->task->description, sizeof(s->desc) - 1);
        s->desc[sizeof(s->desc) - 1] = '\0';
        s->priority = (int)s->task->priority;
        s->status   = (int)s->task->status;

        if (s->task->due_date) {
          char due_buf[32];
          snprintf(due_buf, sizeof(due_buf), "%u", s->task->due_date);
          set_window_item_text(win, ID_TASK_DUEDATE_CTRL, "%s", due_buf);
        }
      }
      dialog_push(win, s, STATIC_ARRAY(k_task_bindings));
      return true;
    }

    case evCommand:
      if (HIWORD(wparam) == btnClicked) {
        window_t *src = (window_t *)lparam;

        if (src->id == ID_OK) {
          // Validate title before accepting.
          window_t *et = get_window_item(win, ID_TASK_TITLE_CTRL);
          if (!et || et->title[0] == '\0') {
            message_box(win, "Title is required.", "Validation", MB_OK);
            return true;
          }

          dialog_pull(win, s, STATIC_ARRAY(k_task_bindings));

          // Due date: optional uint32_t — not in binding table (needs custom parsing).
          window_t *edue = get_window_item(win, ID_TASK_DUEDATE_CTRL);
          s->due_date = 0;
          if (edue && edue->title[0] != '\0') {
            char *endp = NULL;
            unsigned long parsed = strtoul(edue->title, &endp, 10);
            if (*endp != '\0') {
              message_box(win, "Due date must be a Unix timestamp (e.g. 1735689600) or empty for none.",
                          "Validation", MB_OK);
              return true;
            }
            s->due_date = (uint32_t)parsed;
          }

          s->accepted = true;
          end_dialog(win, 1);
          return true;
        }
        if (src->id == ID_CANCEL) {
          end_dialog(win, 0);
          return true;
        }
      }
      return false;

    default:
      return default_winproc(win, msg, wparam, lparam);
  }
}

// ============================================================
// Public entry point
// ============================================================

bool show_task_dialog(window_t *parent, task_t *task) {
  task_doc_t *doc = doc_from_window(parent);
  task_dlg_state_t state = {
    .task     = task,
    .accepted = false,
    .priority = PRIORITY_NORMAL,
    .status   = STATUS_TODO,
    .due_date = 0,
  };
  state.title[0] = '\0';
  state.desc[0]  = '\0';

  const char *dlg_title = task ? "Edit Task" : "New Task";
  show_dialog_from_form(&kTaskEditForm, dlg_title, parent,
                        task_dlg_proc, &state);

  if (!state.accepted) return false;

  if (task) {
    // Edit mode: update existing task.
    task_update(task, state.title, state.desc,
                (task_priority_t)state.priority,
                (task_status_t)state.status,
                state.due_date);
    if (doc) doc->modified = true;
  } else {
    // Create mode: build a new task and add to app state.
    task_t *t = task_create(state.title, state.desc,
                            (task_priority_t)state.priority,
                            (task_status_t)state.status,
                            state.due_date);
    if (t && doc) app_add_task(doc, t);
  }
  return true;
}
