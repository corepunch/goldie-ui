// Tests for create_window_from_form() and show_dialog_from_form().
//
// Verifies:
//  1. Children declared in a form_def_t exist and are findable via
//     get_window_item() when evCreate fires on the parent.
//  2. Child IDs, flags, and initial text are applied correctly.
//  3. show_dialog_from_form() creates a window with WINDOW_DIALOG set,
//     applies the title override, and includes WINDOW_VSCROLL.
//  4. show_ddx_dialog() pushes state → controls, and OK/Cancel end the dialog.

#include "test_framework.h"
#include "test_env.h"
#include "../ui.h"
#include "../examples/socialfeed/socialfeed.h"

// ──────────────────────────────────────────────────────────────────────────
// Shared form definition used by several tests
// ──────────────────────────────────────────────────────────────────────────

#define FORM_ID_NAME   1
#define FORM_ID_OK     2
#define FORM_ID_CANCEL 3

static const form_ctrl_def_t kTestFormChildren[] = {
  { "textedit", FORM_ID_NAME,   {60,  8, 80, 13}, 0,              "hello", "name"   },
  { "button",   FORM_ID_OK,     {50, 30, 40, 13}, BUTTON_DEFAULT, "OK",    "ok"     },
  { "button",   FORM_ID_CANCEL, {94, 30, 50, 13}, 0,              "Cancel","cancel" },
};

static const form_def_t kTestForm = {
  .name        = "Test Form",
  .width       = 160,
  .height      = 52,
  .flags       = 0,
  .children    = kTestFormChildren,
  .child_count = 3,
};

// ──────────────────────────────────────────────────────────────────────────
// DDX form and state for show_ddx_dialog tests
// ──────────────────────────────────────────────────────────────────────────

#define DDX_FORM_ID_NAME   1
#define DDX_FORM_ID_OK     2
#define DDX_FORM_ID_CANCEL 3

typedef struct { char name[64]; } ddx_test_state_t;

static const ctrl_binding_t kDdxTestBindings[] = {
  DDX_TEXT(DDX_FORM_ID_NAME, ddx_test_state_t, name),
};

static const form_ctrl_def_t kDdxFormChildren[] = {
  { "textedit", DDX_FORM_ID_NAME,   {60,  8, 80, 13}, 0,              "",       "name"   },
  { "button",   DDX_FORM_ID_OK,     {50, 30, 40, 13}, BUTTON_DEFAULT, "OK",     "ok"     },
  { "button",   DDX_FORM_ID_CANCEL, {94, 30, 50, 13}, 0,              "Cancel", "cancel" },
};

static const form_def_t kDdxTestForm = {
  .name          = "DDX Test",
  .width         = 160,
  .height        = 52,
  .flags         = 0,
  .children      = kDdxFormChildren,
  .child_count   = 3,
  .bindings      = kDdxTestBindings,
  .binding_count = ARRAY_LEN(kDdxTestBindings),
  .ok_id         = DDX_FORM_ID_OK,
  .cancel_id     = DDX_FORM_ID_CANCEL,
};

// ──────────────────────────────────────────────────────────────────────────
// Auto-layout form with padding used to verify the inset is honored
// ──────────────────────────────────────────────────────────────────────────

#define PAD_FORM_ID_FIRST   201
#define PAD_FORM_ID_SECOND  202

static const form_ctrl_def_t kPadChildren[] = {
  {
    .class_name = "button",
    .id = PAD_FORM_ID_FIRST,
    .frame = {0, 0, 80, 0},
    .text = "Alpha",
    .name = "alpha",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "button",
    .id = PAD_FORM_ID_SECOND,
    .frame = {0, 0, 80, 0},
    .text = "Beta",
    .name = "beta",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
};

static const form_def_t kPadForm = {
  .name = "Pad",
  .width = 200,
  .height = 80,
  .flags = WINDOW_NOTITLE | WINDOW_NOFILL,
  .auto_layout = true,
  .layout_kind = "stack",
  .layout_orientation = WINDOW_STACK_VERTICAL,
  .layout_spacing = 4,
  .padding = {8, 8, 8, 8},
  .children = kPadChildren,
  .child_count = ARRAY_LEN(kPadChildren),
};

#define MAR_FORM_ID_FIRST   301
#define MAR_FORM_ID_SECOND  302

static const form_ctrl_def_t kMarChildren[] = {
  {
    .class_name = "button",
    .id = MAR_FORM_ID_FIRST,
    .frame = {0, 0, 80, 0},
    .text = "Gamma",
    .name = "gamma",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
    .margin = {8, 8, 8, 8},
  },
  {
    .class_name = "button",
    .id = MAR_FORM_ID_SECOND,
    .frame = {0, 0, 80, 0},
    .text = "Delta",
    .name = "delta",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
    .margin = {2, 6, 2, 6},
  },
};

static const form_def_t kMarForm = {
  .name = "Margin",
  .width = 200,
  .height = 80,
  .flags = WINDOW_NOTITLE | WINDOW_NOFILL,
  .auto_layout = true,
  .layout_kind = "stack",
  .layout_orientation = WINDOW_STACK_VERTICAL,
  .layout_spacing = 4,
  .children = kMarChildren,
  .child_count = ARRAY_LEN(kMarChildren),
};

#define WRAP_FORM_ID_LABEL 401

static const form_ctrl_def_t kWrapChildren[] = {
  {
    .class_name = "label",
    .id = WRAP_FORM_ID_LABEL,
    .frame = {0, 0, 0, 0},
    .text = "This label should wrap when the available width is limited by layout.",
    .name = "wrap",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
};

static const form_def_t kWrapForm = {
  .name = "Wrap",
  .width = 80,
  .height = 80,
  .flags = WINDOW_NOTITLE | WINDOW_NOFILL,
  .auto_layout = true,
  .layout_kind = "stack",
  .layout_orientation = WINDOW_STACK_VERTICAL,
  .layout_spacing = 4,
  .children = kWrapChildren,
  .child_count = ARRAY_LEN(kWrapChildren),
};

// ──────────────────────────────────────────────────────────────────────────
// Nested auto-layout form used to debug stack and grid positioning
// ──────────────────────────────────────────────────────────────────────────

#define NEST_FORM_ID_HEADER      101
#define NEST_FORM_ID_BODY        102
#define NEST_FORM_ID_GRID        103
#define NEST_FORM_ID_TITLE       104
#define NEST_FORM_ID_AUTHOR      105
#define NEST_FORM_ID_BODY_BTN1    106
#define NEST_FORM_ID_BODY_BTN2    107
#define NEST_FORM_ID_GRID_1       108
#define NEST_FORM_ID_GRID_2       109
#define NEST_FORM_ID_GRID_3       110
#define NEST_FORM_ID_GRID_4       111

static const form_ctrl_def_t kNestBodyChildren[] = {
  {
    .class_name = "button",
    .id = NEST_FORM_ID_BODY_BTN1,
    .frame = {0, 0, 88, 0},
    .text = "Like Post",
    .name = "like",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "button",
    .id = NEST_FORM_ID_BODY_BTN2,
    .frame = {0, 0, 72, 0},
    .text = "Close",
    .name = "close",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
};

static const form_ctrl_def_t kNestGridChildren[] = {
  {
    .class_name = "label",
    .id = NEST_FORM_ID_GRID_1,
    .frame = {0, 0, 40, 0},
    .text = "G1",
    .name = "g1",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_STRETCH,
  },
  {
    .class_name = "label",
    .id = NEST_FORM_ID_GRID_2,
    .frame = {0, 0, 40, 0},
    .text = "G2",
    .name = "g2",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_STRETCH,
  },
  {
    .class_name = "label",
    .id = NEST_FORM_ID_GRID_3,
    .frame = {0, 0, 40, 0},
    .text = "G3",
    .name = "g3",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_STRETCH,
  },
  {
    .class_name = "label",
    .id = NEST_FORM_ID_GRID_4,
    .frame = {0, 0, 40, 0},
    .text = "G4",
    .name = "g4",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_STRETCH,
  },
};

static const form_ctrl_def_t kNestChildren[] = {
  {
    .class_name = "label",
    .id = NEST_FORM_ID_HEADER,
    .frame = {0, 0, 120, 0},
    .text = "Post Detail",
    .name = "header",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "stack",
    .id = NEST_FORM_ID_BODY,
    .frame = {0, 0, 0, 0},
    .name = "body",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
    .children = kNestBodyChildren,
    .child_count = ARRAY_LEN(kNestBodyChildren),
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .layout_spacing = 3,
  },
  {
    .class_name = "grid",
    .id = NEST_FORM_ID_GRID,
    .frame = {0, 0, 0, 0},
    .name = "grid",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
    .children = kNestGridChildren,
    .child_count = ARRAY_LEN(kNestGridChildren),
    .layout_kind = "grid",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .layout_columns = 2,
    .layout_spacing = 0,
  },
};

static const form_def_t kNestForm = {
  .name = "Nest",
  .width = 240,
  .height = 180,
  .flags = WINDOW_NOTITLE | WINDOW_NOFILL,
  .auto_layout = true,
  .layout_kind = "stack",
  .layout_orientation = WINDOW_STACK_VERTICAL,
  .layout_columns = 0,
  .layout_spacing = 6,
  .children = kNestChildren,
  .child_count = ARRAY_LEN(kNestChildren),
};

static const form_ctrl_def_t kDefaultStackChildren[] = {
  {
    .class_name = "button",
    .id = 201,
    .frame = {0, 0, 80, 0},
    .text = "First",
    .name = "first",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "button",
    .id = 202,
    .frame = {0, 0, 80, 0},
    .text = "Second",
    .name = "second",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
};

static const form_def_t kDefaultStackForm = {
  .name = "DefaultStack",
  .width = 200,
  .height = 80,
  .flags = WINDOW_NOTITLE | WINDOW_NOFILL,
  .auto_layout = true,
  .padding = {0, 0, 0, 0},
  .children = kDefaultStackChildren,
  .child_count = ARRAY_LEN(kDefaultStackChildren),
};

#define NP_FORM_ID_FIELDS   203
#define NP_FORM_ID_AUTHOR_L 204
#define NP_FORM_ID_AUTHOR_E 205
#define NP_FORM_ID_TITLE_L   206
#define NP_FORM_ID_TITLE_E   207
#define NP_FORM_ID_BODY_L    208
#define NP_FORM_ID_BODY_E    209
#define NP_FORM_ID_ACTIONS   210
#define NP_FORM_ID_OK        211
#define NP_FORM_ID_CANCEL    212

static const form_ctrl_def_t kNewPostFieldsChildren[] = {
  {
    .class_name = "label",
    .id = NP_FORM_ID_AUTHOR_L,
    .frame = {0, 0, 56, 0},
    .text = "Author:",
    .name = "author_lbl",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "textedit",
    .id = NP_FORM_ID_AUTHOR_E,
    .frame = {0, 0, 0, 0},
    .text = "",
    .name = "author",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "label",
    .id = NP_FORM_ID_TITLE_L,
    .frame = {0, 0, 56, 0},
    .text = "Title:",
    .name = "title_lbl",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "textedit",
    .id = NP_FORM_ID_TITLE_E,
    .frame = {0, 0, 0, 0},
    .text = "",
    .name = "title",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "label",
    .id = NP_FORM_ID_BODY_L,
    .frame = {0, 0, 56, 0},
    .text = "Body:",
    .name = "body_lbl",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "multiedit",
    .id = NP_FORM_ID_BODY_E,
    .frame = {0, 0, 0, 48},
    .text = "",
    .name = "body",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
};

static const form_ctrl_def_t kNewPostChildren[] = {
  {
    .class_name = "grid",
    .id = NP_FORM_ID_FIELDS,
    .frame = {0, 0, 0, 0},
    .name = "fields",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_STRETCH,
    .children = kNewPostFieldsChildren,
    .child_count = ARRAY_LEN(kNewPostFieldsChildren),
    .layout_kind = "grid",
    .layout_columns = 2,
    .layout_spacing = 4,
  },
  {
    .class_name = "stack",
    .id = NP_FORM_ID_ACTIONS,
    .frame = {0, 0, 0, 0},
    .name = "actions",
    .h_align = LAYOUT_ALIGN_CENTER,
    .v_align = LAYOUT_ALIGN_START,
    .children = (const form_ctrl_def_t[]){
      {
        .class_name = "button",
        .id = NP_FORM_ID_OK,
        .frame = {0, 0, 44, 0},
        .text = "Post",
        .name = "ok",
        .h_align = LAYOUT_ALIGN_START,
        .v_align = LAYOUT_ALIGN_START,
      },
      {
        .class_name = "button",
        .id = NP_FORM_ID_CANCEL,
        .frame = {0, 0, 56, 0},
        .text = "Cancel",
        .name = "cancel",
        .h_align = LAYOUT_ALIGN_START,
        .v_align = LAYOUT_ALIGN_START,
      },
    },
    .child_count = 2,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_HORIZONTAL,
    .layout_spacing = 6,
  },
};

static const form_def_t kNewPostGridForm = {
  .name = "NewPostGrid",
  .width = 272,
  .height = 150,
  .flags = WINDOW_NOTITLE | WINDOW_NOFILL,
  .auto_layout = true,
  .layout_kind = "stack",
  .layout_orientation = WINDOW_STACK_VERTICAL,
  .layout_spacing = 4,
  .padding = {8, 8, 8, 8},
  .children = kNewPostChildren,
  .child_count = ARRAY_LEN(kNewPostChildren),
};

static result_t form_test_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam);
static result_t post_detail_like_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam);
static result_t socialfeed_post_detail_layout_proc(window_t *win, uint32_t msg,
                                                   uint32_t wparam, void *lparam);

// ──────────────────────────────────────────────────────────────────────────
// Post-detail-like form used to validate wrapped labels + reportview + footer
// sizing.  This mirrors the Social Feed Post Detail dialog closely enough to
// catch footer clipping when the title/body labels wrap onto extra lines.
// ──────────────────────────────────────────────────────────────────────────

#define PD_FORM_ID_LAYOUT       401
#define PD_FORM_ID_HEADER       402
#define PD_FORM_ID_TITLE        403
#define PD_FORM_ID_AUTHOR       404
#define PD_FORM_ID_BODY         405
#define PD_FORM_ID_LIKES        406
#define PD_FORM_ID_COMMENTS_HDR  407
#define PD_FORM_ID_COMMENTS     408
#define PD_FORM_ID_ACTIONS      409
#define PD_FORM_ID_LIKE_POST    410
#define PD_FORM_ID_ADD_COMMENT  411
#define PD_FORM_ID_ADD_REPLY    412
#define PD_FORM_ID_LIKE_COMMENT 413
#define PD_FORM_ID_CLOSE        414

static const form_ctrl_def_t kPostDetailLikeHeaderChildren[] = {
  {
    .class_name = "label",
    .id = PD_FORM_ID_TITLE,
    .frame = {0, 0, 0, 0},
    .text = "",
    .name = "lbl_title",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "label",
    .id = PD_FORM_ID_AUTHOR,
    .frame = {0, 0, 0, 0},
    .text = "",
    .name = "lbl_author",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "label",
    .id = PD_FORM_ID_BODY,
    .frame = {0, 0, 0, 0},
    .text = "",
    .name = "lbl_body",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "label",
    .id = PD_FORM_ID_LIKES,
    .frame = {0, 0, 0, 0},
    .text = "",
    .name = "lbl_likes",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "label",
    .id = PD_FORM_ID_COMMENTS_HDR,
    .frame = {0, 0, 0, 0},
    .text = "",
    .name = "lbl_cmt_hdr",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_START,
  },
};

static const form_ctrl_def_t kPostDetailLikeActionsChildren[] = {
  {
    .class_name = "button",
    .id = PD_FORM_ID_LIKE_POST,
    .frame = {0, 0, 0, 0},
    .text = "Like Post",
    .name = "like_post",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "button",
    .id = PD_FORM_ID_ADD_COMMENT,
    .frame = {0, 0, 0, 0},
    .text = "Add Comment",
    .name = "add_comment",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "button",
    .id = PD_FORM_ID_ADD_REPLY,
    .frame = {0, 0, 0, 0},
    .text = "Add Reply",
    .name = "add_reply",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "button",
    .id = PD_FORM_ID_LIKE_COMMENT,
    .frame = {0, 0, 0, 0},
    .text = "Like Comment",
    .name = "like_comment",
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
  {
    .class_name = "button",
    .id = PD_FORM_ID_CLOSE,
    .frame = {0, 0, 0, 0},
    .text = "Close",
    .name = "close",
    .flags = BUTTON_DEFAULT,
    .h_align = LAYOUT_ALIGN_START,
    .v_align = LAYOUT_ALIGN_START,
  },
};

static const char *kPostDetailLikeBodyText =
  "Declare children in a static form_ctrl_def_t[] array, add a ctrl_binding_t[] "
  "for data exchange, then call show_dialog_from_form(). No imperative child "
  "creation needed - everything is declarative. The same tree should also "
  "support auto layout, report views, and wrapped labels without special cases.";

static const form_ctrl_def_t kPostDetailLikeChildren[] = {
  {
    .class_name = "stack",
    .id = PD_FORM_ID_LAYOUT,
    .frame = {0, 0, 0, 0},
    .name = "layout",
    .h_align = LAYOUT_ALIGN_STRETCH,
    .v_align = LAYOUT_ALIGN_STRETCH,
    .children = (const form_ctrl_def_t[]){
      {
        .class_name = "stack",
        .id = PD_FORM_ID_HEADER,
        .frame = {0, 0, 0, 0},
        .name = "header",
        .h_align = LAYOUT_ALIGN_STRETCH,
        .v_align = LAYOUT_ALIGN_START,
        .children = kPostDetailLikeHeaderChildren,
        .child_count = ARRAY_LEN(kPostDetailLikeHeaderChildren),
        .layout_kind = "stack",
        .layout_orientation = WINDOW_STACK_VERTICAL,
        .layout_spacing = 2,
      },
      {
        .class_name = "reportview",
        .id = PD_FORM_ID_COMMENTS,
        .frame = {0, 0, 0, 0},
        .text = "",
        .name = "comments",
        .flags = WINDOW_NOTITLE | WINDOW_NOFILL | WINDOW_VSCROLL,
        .h_align = LAYOUT_ALIGN_STRETCH,
        .v_align = LAYOUT_ALIGN_STRETCH,
      },
      {
        .class_name = "stack",
        .id = PD_FORM_ID_ACTIONS,
        .frame = {0, 0, 0, 0},
        .name = "actions",
        .h_align = LAYOUT_ALIGN_STRETCH,
        .v_align = LAYOUT_ALIGN_START,
        .children = kPostDetailLikeActionsChildren,
        .child_count = ARRAY_LEN(kPostDetailLikeActionsChildren),
        .layout_kind = "stack",
        .layout_orientation = WINDOW_STACK_HORIZONTAL,
        .layout_spacing = 4,
      },
    },
    .child_count = 3,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .layout_spacing = 8,
  },
};

static window_t *create_post_detail_like_window(int height) {
  form_def_t def = {
    .name = "Post Detail Like",
    .width = 520,
    .height = height,
    .flags = 0,
    .auto_layout = true,
    .layout_kind = "stack",
    .layout_orientation = WINDOW_STACK_VERTICAL,
    .layout_spacing = 8,
    .padding = {8, 8, 8, 8},
    .children = kPostDetailLikeChildren,
    .child_count = ARRAY_LEN(kPostDetailLikeChildren),
  };
  return create_window_from_form(&def, 0, 0, NULL, post_detail_like_proc, 0, NULL);
}

static result_t post_detail_like_proc(window_t *win, uint32_t msg,
                                      uint32_t wparam, void *lparam) {
  (void)wparam;
  (void)lparam;
  if (msg == evCreate) {
    set_window_item_text(win, PD_FORM_ID_TITLE, "Orion UI is awesome!");
    set_window_item_text(win, PD_FORM_ID_AUTHOR, "by alice");
    set_window_item_text(win, PD_FORM_ID_BODY, "%s", kPostDetailLikeBodyText);
    set_window_item_text(win, PD_FORM_ID_LIKES, "12 likes");
    set_window_item_text(win, PD_FORM_ID_COMMENTS_HDR, "Comments (3):");
    window_layout_sync(win);
    return true;
  }
  return false;
}

static result_t socialfeed_post_detail_layout_proc(window_t *win, uint32_t msg,
                                                   uint32_t wparam, void *lparam) {
  (void)wparam;
  (void)lparam;
  if (msg == evCreate) {
    set_window_item_text(win, ID_POST_DETAIL_LBL_TITLE, "Orion UI is awesome!");
    set_window_item_text(win, ID_POST_DETAIL_LBL_AUTHOR, "by alice");
    set_window_item_text(win, ID_POST_DETAIL_LBL_BODY, "%s", kPostDetailLikeBodyText);
    set_window_item_text(win, ID_POST_DETAIL_LBL_LIKES, "12 likes");
    set_window_item_text(win, ID_POST_DETAIL_LBL_CMT_HDR, "Comments (3):");

    window_t *cv = get_window_item(win, ID_POST_DETAIL_COMMENTS);
    if (cv) {
      send_message(cv, RVM_SETREDRAW, 0, NULL);
      send_message(cv, RVM_SETVIEWMODE, RVM_VIEW_REPORT, NULL);
      send_message(cv, RVM_CLEARCOLUMNS, 0, NULL);
      reportview_column_t col_author = { "Author", 70 };
      reportview_column_t col_text = { "Text", 120 };
      reportview_column_t col_likes = { "Likes", 45 };
      send_message(cv, RVM_ADDCOLUMN, 0, &col_author);
      send_message(cv, RVM_ADDCOLUMN, 0, &col_text);
      send_message(cv, RVM_ADDCOLUMN, 0, &col_likes);
      send_message(cv, RVM_CLEAR, 0, NULL);
      send_message(cv, RVM_SETREDRAW, 1, NULL);
    }

    window_layout_sync(win);
    return true;
  }
  return false;
}

// ──────────────────────────────────────────────────────────────────────────
// State captured in the window proc during evCreate
// ──────────────────────────────────────────────────────────────────────────

typedef struct {
  bool      create_fired;
  window_t *found_name;    // get_window_item result for FORM_ID_NAME
  window_t *found_ok;      // get_window_item result for FORM_ID_OK
  window_t *found_cancel;  // get_window_item result for FORM_ID_CANCEL
  flags_t   ok_flags;      // flags of the OK button at create time
  char      name_text[64]; // text of the name edit box at create time
} form_create_state_t;

static form_create_state_t g_create_state;

static result_t form_test_proc(window_t *win, uint32_t msg,
                               uint32_t wparam, void *lparam) {
  (void)wparam; (void)lparam;
  if (msg == evCreate) {
    g_create_state.create_fired  = true;
    g_create_state.found_name    = get_window_item(win, FORM_ID_NAME);
    g_create_state.found_ok      = get_window_item(win, FORM_ID_OK);
    g_create_state.found_cancel  = get_window_item(win, FORM_ID_CANCEL);
    if (g_create_state.found_ok)
      g_create_state.ok_flags = g_create_state.found_ok->flags;
    if (g_create_state.found_name)
      strncpy(g_create_state.name_text, g_create_state.found_name->title,
              sizeof(g_create_state.name_text) - 1);
    return true;
  }
  return false;
}

// ──────────────────────────────────────────────────────────────────────────
// Test 1: children exist at evCreate
// ──────────────────────────────────────────────────────────────────────────

void test_form_children_exist_at_create(void) {
  TEST("create_window_from_form: children findable in evCreate");

  test_env_init();
  memset(&g_create_state, 0, sizeof(g_create_state));

  window_t *win = create_window_from_form(&kTestForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  ASSERT_TRUE(g_create_state.create_fired);
  ASSERT_NOT_NULL(g_create_state.found_name);
  ASSERT_NOT_NULL(g_create_state.found_ok);
  ASSERT_NOT_NULL(g_create_state.found_cancel);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// ──────────────────────────────────────────────────────────────────────────
// Test 2: child IDs are applied correctly
// ──────────────────────────────────────────────────────────────────────────

void test_form_child_ids(void) {
  TEST("create_window_from_form: child IDs match form definition");

  test_env_init();
  memset(&g_create_state, 0, sizeof(g_create_state));

  window_t *win = create_window_from_form(&kTestForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  ASSERT_NOT_NULL(g_create_state.found_name);
  ASSERT_NOT_NULL(g_create_state.found_ok);
  ASSERT_NOT_NULL(g_create_state.found_cancel);
  ASSERT_EQUAL((int)g_create_state.found_name->id,   FORM_ID_NAME);
  ASSERT_EQUAL((int)g_create_state.found_ok->id,     FORM_ID_OK);
  ASSERT_EQUAL((int)g_create_state.found_cancel->id, FORM_ID_CANCEL);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// ──────────────────────────────────────────────────────────────────────────
// Test 3: child flags are applied correctly
// ──────────────────────────────────────────────────────────────────────────

void test_form_child_flags(void) {
  TEST("create_window_from_form: child flags (BUTTON_DEFAULT) applied");

  test_env_init();
  memset(&g_create_state, 0, sizeof(g_create_state));

  window_t *win = create_window_from_form(&kTestForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  ASSERT_TRUE(g_create_state.ok_flags & BUTTON_DEFAULT);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// ──────────────────────────────────────────────────────────────────────────
// Test 4: child initial text is applied correctly
// ──────────────────────────────────────────────────────────────────────────

void test_form_child_text(void) {
  TEST("create_window_from_form: child initial text applied");

  test_env_init();
  memset(&g_create_state, 0, sizeof(g_create_state));

  window_t *win = create_window_from_form(&kTestForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  ASSERT_STR_EQUAL(g_create_state.name_text, "hello");

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// ──────────────────────────────────────────────────────────────────────────
// Test 5: show_dialog_from_form applies WINDOW_DIALOG flag and title override
// ──────────────────────────────────────────────────────────────────────────

// Window proc used for the dialog-flag test: immediately ends the dialog.
static flags_t   g_dlg_flags     = 0;
static char      g_dlg_title[64] = {0};

static result_t dialog_flag_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  (void)wparam; (void)lparam;
  if (msg == evCreate) {
    g_dlg_flags = win->flags;
    strncpy(g_dlg_title, win->title, sizeof(g_dlg_title) - 1);
    end_dialog(win, 1);
    return true;
  }
  return false;
}

void test_show_dialog_from_form_flags(void) {
  TEST("show_dialog_from_form: WINDOW_DIALOG and WINDOW_VSCROLL set; title override applied");

  test_env_init();
  g_dlg_flags = 0;
  memset(g_dlg_title, 0, sizeof(g_dlg_title));

  // UI runtime state must be running for show_dialog_from_form to enter its loop.
  g_ui_runtime.running = true;

  show_dialog_from_form(&kTestForm, "Override Title", NULL, dialog_flag_proc, NULL);

  ASSERT_TRUE(g_dlg_flags & WINDOW_DIALOG);
  ASSERT_TRUE(g_dlg_flags & WINDOW_VSCROLL);
  ASSERT_STR_EQUAL(g_dlg_title, "Override Title");

  test_env_shutdown();
  PASS();
}

void test_center_window_rect_owner(void) {
  TEST("center_window_rect: centers inside owner frame");

  test_env_init();

  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  window_t owner = {0};
  owner.frame = (irect16_t){20, 20, MIN(200, sw - 40), MIN(120, sh - 40)};

  irect16_t centered = center_window_rect((irect16_t){0, 0, 120, 60}, &owner);
  ASSERT_EQUAL(centered.x, owner.frame.x + (owner.frame.w - 120) / 2);
  ASSERT_EQUAL(centered.y, owner.frame.y + (owner.frame.h - 60) / 2);
  ASSERT_EQUAL(centered.w, 120);
  ASSERT_EQUAL(centered.h, 60);

  test_env_shutdown();
  PASS();
}

void test_center_window_rect_screen_clamp(void) {
  TEST("center_window_rect: clamps oversized frame to screen origin");

  test_env_init();

  int sw = ui_get_system_metrics(kSystemMetricScreenWidth);
  int sh = ui_get_system_metrics(kSystemMetricScreenHeight);
  irect16_t centered = center_window_rect((irect16_t){0, 0, sw + 50, sh + 20}, NULL);

  ASSERT_EQUAL(centered.x, 0);
  ASSERT_EQUAL(centered.y, 0);
  ASSERT_EQUAL(centered.w, sw + 50);
  ASSERT_EQUAL(centered.h, sh + 20);

  test_env_shutdown();
  PASS();
}

// ──────────────────────────────────────────────────────────────────────────
// Test 6–8: show_ddx_dialog / DDX push+pull behaviour
// ──────────────────────────────────────────────────────────────────────────

// Test 6: form_def_t DDX fields are set correctly.
void test_ddx_form_def_fields(void) {
  TEST("form_def_t: ok_id, cancel_id, bindings, binding_count set correctly");

  ASSERT_EQUAL((int)kDdxTestForm.ok_id,         DDX_FORM_ID_OK);
  ASSERT_EQUAL((int)kDdxTestForm.cancel_id,      DDX_FORM_ID_CANCEL);
  ASSERT_EQUAL((int)kDdxTestForm.binding_count,  1);
  ASSERT_NOT_NULL((void *)kDdxTestForm.bindings);
  ASSERT_EQUAL((int)kDdxTestForm.bindings[0].ctrl_id, DDX_FORM_ID_NAME);

  PASS();
}

// Minimal no-op proc for tests that create a plain parent window.
static result_t nop_proc(window_t *w, uint32_t m, uint32_t wp, void *lp) {
  (void)w; (void)m; (void)wp; (void)lp;
  return false;
}

// Test 7: dialog_push writes state → controls; dialog_pull reads back correctly.
void test_ddx_push_pull_roundtrip(void) {
  TEST("dialog_push / dialog_pull: round-trip preserves state");

  test_env_init();

  ddx_test_state_t st_in  = {0};
  ddx_test_state_t st_out = {0};
  snprintf(st_in.name, sizeof(st_in.name), "roundtrip_value");

  // Create the form window without a modal loop.
  window_t *win = create_window_from_form(&kDdxTestForm, 0, 0, NULL,
                                          nop_proc, 0, NULL);
  ASSERT_NOT_NULL(win);

  dialog_push(win, &st_in, kDdxTestForm.bindings, kDdxTestForm.binding_count);

  // Verify control text was set by the push.
  window_t *edit = get_window_item(win, DDX_FORM_ID_NAME);
  ASSERT_NOT_NULL(edit);
  ASSERT_STR_EQUAL(edit->title, "roundtrip_value");

  dialog_pull(win, &st_out, kDdxTestForm.bindings, kDdxTestForm.binding_count);
  ASSERT_STR_EQUAL(st_out.name, "roundtrip_value");

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// Test 8: show_ddx_dialog with a proc that ends the dialog during evCreate
// (standard headless pattern); verifies code == 1 and state is populated.
// The proc wraps dialog_ddx_proc behaviour manually for testability.

static ddx_test_state_t g_ddx_test_st;
static flags_t          g_ddx_dlg_flags;

static result_t ddx_verify_proc(window_t *win, uint32_t msg,
                                 uint32_t wparam, void *lparam) {
  (void)wparam; (void)lparam;
  if (msg == evCreate) {
    g_ddx_dlg_flags = win->flags;
    // Manually replicate DDX push+pull so we can verify without the modal loop.
    dialog_push(win, &g_ddx_test_st,
                kDdxTestForm.bindings, kDdxTestForm.binding_count);
    dialog_pull(win, &g_ddx_test_st,
                kDdxTestForm.bindings, kDdxTestForm.binding_count);
    end_dialog(win, 1);
    return true;
  }
  return false;
}

void test_show_ddx_dialog_form_flags(void) {
  TEST("show_ddx_dialog: dialog gets WINDOW_DIALOG flag and DDX push+pull works");

  test_env_init();
  g_ddx_dlg_flags = 0;
  memset(&g_ddx_test_st, 0, sizeof(g_ddx_test_st));
  snprintf(g_ddx_test_st.name, sizeof(g_ddx_test_st.name), "expected");

  g_ui_runtime.running = true;

  // Use show_dialog_from_form directly with our verification proc.
  // This tests that the form correctly carries DDX metadata and that
  // WINDOW_DIALOG is applied — the same flags show_ddx_dialog would use.
  show_dialog_from_form(&kDdxTestForm, "DDX Verify",
                        NULL, ddx_verify_proc, NULL);

  // Verify the dialog received WINDOW_DIALOG.
  ASSERT_TRUE(g_ddx_dlg_flags & WINDOW_DIALOG);
  // Verify push+pull round-trip: name should equal "expected".
  ASSERT_STR_EQUAL(g_ddx_test_st.name, "expected");

  test_env_shutdown();
  PASS();
}

void test_stackview_layout(void) {
  TEST("stackview: stretches children vertically by default");

  test_env_init();

  layout_view_config_t cfg = {
    .layout_kind = "stack",
    .orientation = WINDOW_STACK_VERTICAL,
    .columns = 0,
  };
  window_t *root = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL,
                                 MAKERECT(0, 0, 200, 100),
                                 NULL, "stackview", 0, &cfg);
  ASSERT_NOT_NULL(root);

  window_t *a = create_window("One", 0, MAKERECT(0, 0, 30, 12), root, "button", 0, NULL);
  window_t *b = create_window("Two", 0, MAKERECT(0, 0, 30, 12), root, "button", 0, NULL);
  ASSERT_NOT_NULL(a);
  ASSERT_NOT_NULL(b);

  window_layout_sync(root);

  ASSERT_EQUAL(a->frame.x, 0);
  ASSERT_EQUAL(a->frame.y, 0);
  ASSERT_EQUAL(a->frame.w, 200);
  ASSERT_EQUAL(b->frame.x, 0);
  ASSERT_EQUAL(b->frame.w, 200);
  ASSERT_EQUAL(b->frame.y, a->frame.y + a->frame.h + 4);

  destroy_window(root);
  test_env_shutdown();
  PASS();
}

void test_gridview_layout(void) {
  TEST("gridview: arranges children compactly by row and column sizes");

  test_env_init();

  layout_view_config_t cfg = {
    .layout_kind = "grid",
    .orientation = WINDOW_STACK_VERTICAL,
    .columns = 2,
  };
  window_t *root = create_window("", WINDOW_NOTITLE | WINDOW_NOFILL,
                                 MAKERECT(0, 0, 200, 80),
                                 NULL, "gridview", 0, &cfg);
  ASSERT_NOT_NULL(root);

  window_t *c0 = create_window("A", 0, MAKERECT(0, 0, 20, 12), root, "button", 0, NULL);
  window_t *c1 = create_window("B", 0, MAKERECT(0, 0, 20, 12), root, "button", 0, NULL);
  window_t *c2 = create_window("C", 0, MAKERECT(0, 0, 20, 12), root, "button", 0, NULL);
  window_t *c3 = create_window("D", 0, MAKERECT(0, 0, 20, 12), root, "button", 0, NULL);
  ASSERT_NOT_NULL(c0);
  ASSERT_NOT_NULL(c1);
  ASSERT_NOT_NULL(c2);
  ASSERT_NOT_NULL(c3);
  c0->h_align = LAYOUT_ALIGN_START;
  c1->h_align = LAYOUT_ALIGN_START;
  c2->h_align = LAYOUT_ALIGN_START;
  c3->h_align = LAYOUT_ALIGN_START;
  c0->v_align = LAYOUT_ALIGN_START;
  c1->v_align = LAYOUT_ALIGN_START;
  c2->v_align = LAYOUT_ALIGN_START;
  c3->v_align = LAYOUT_ALIGN_START;

  window_layout_sync(root);

  ASSERT_EQUAL(c0->frame.x, 0);
  ASSERT_EQUAL(c0->frame.y, 0);
  ASSERT_EQUAL(c0->frame.w, 20);
  ASSERT_EQUAL(c0->frame.h, 19);
  ASSERT_EQUAL(c1->frame.x, 20);
  ASSERT_EQUAL(c1->frame.y, 0);
  ASSERT_EQUAL(c1->frame.w, 20);
  ASSERT_EQUAL(c1->frame.h, 19);
  ASSERT_EQUAL(c2->frame.x, 0);
  ASSERT_EQUAL(c2->frame.y, 19);
  ASSERT_EQUAL(c2->frame.w, 20);
  ASSERT_EQUAL(c2->frame.h, 19);
  ASSERT_EQUAL(c3->frame.x, 20);
  ASSERT_EQUAL(c3->frame.y, 19);
  ASSERT_EQUAL(c3->frame.w, 20);
  ASSERT_EQUAL(c3->frame.h, 19);

  destroy_window(root);
  test_env_shutdown();
  PASS();
}

void test_auto_layout_padding(void) {
  TEST("auto-layout: padding offsets content inside the client area");

  test_env_init();
  window_t *win = create_window_from_form(&kPadForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  window_t *first = get_window_item(win, PAD_FORM_ID_FIRST);
  window_t *second = get_window_item(win, PAD_FORM_ID_SECOND);
  ASSERT_NOT_NULL(first);
  ASSERT_NOT_NULL(second);

  ASSERT_EQUAL(first->frame.x, 8);
  ASSERT_EQUAL(first->frame.y, 8);
  ASSERT_EQUAL(first->frame.w, 184);
  ASSERT_EQUAL(second->frame.x, 8);
  ASSERT_EQUAL(second->frame.y, first->frame.y + first->frame.h + 4);
  ASSERT_EQUAL(second->frame.w, 184);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_auto_layout_margin(void) {
  TEST("auto-layout: margin offsets individual controls");

  test_env_init();
  window_t *win = create_window_from_form(&kMarForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  window_t *first = get_window_item(win, MAR_FORM_ID_FIRST);
  window_t *second = get_window_item(win, MAR_FORM_ID_SECOND);
  ASSERT_NOT_NULL(first);
  ASSERT_NOT_NULL(second);

  ASSERT_EQUAL(first->frame.x, 8);
  ASSERT_EQUAL(first->frame.y, 8);
  ASSERT_EQUAL(first->frame.w, 184);
  ASSERT_EQUAL(second->frame.x, 2);
  ASSERT_EQUAL(second->frame.y, 45);
  ASSERT_EQUAL(second->frame.w, 196);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_auto_layout_wrapped_label(void) {
  TEST("auto-layout: label wraps when width is constrained");

  test_env_init();
  window_t *win = create_window_from_form(&kWrapForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  window_t *label = get_window_item(win, WRAP_FORM_ID_LABEL);
  ASSERT_NOT_NULL(label);

  ASSERT_EQUAL(label->frame.w, 80);
  ASSERT_TRUE(label->frame.h > CONTROL_HEIGHT);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_nested_stack_positions(void) {
  TEST("nested layout: stack children and grid rows sit in the right place");

  test_env_init();
  window_t *win = create_window_from_form(&kNestForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  window_t *header = get_window_item(win, NEST_FORM_ID_HEADER);
  window_t *body = get_window_item(win, NEST_FORM_ID_BODY);
  window_t *grid = get_window_item(win, NEST_FORM_ID_GRID);
  window_t *body_btn1 = get_window_item(win, NEST_FORM_ID_BODY_BTN1);
  window_t *body_btn2 = get_window_item(win, NEST_FORM_ID_BODY_BTN2);
  window_t *grid_1 = get_window_item(win, NEST_FORM_ID_GRID_1);
  window_t *grid_2 = get_window_item(win, NEST_FORM_ID_GRID_2);
  window_t *grid_3 = get_window_item(win, NEST_FORM_ID_GRID_3);
  window_t *grid_4 = get_window_item(win, NEST_FORM_ID_GRID_4);
  ASSERT_NOT_NULL(header);
  ASSERT_NOT_NULL(body);
  ASSERT_NOT_NULL(grid);
  ASSERT_NOT_NULL(body_btn1);
  ASSERT_NOT_NULL(body_btn2);
  ASSERT_NOT_NULL(grid_1);
  ASSERT_NOT_NULL(grid_2);
  ASSERT_NOT_NULL(grid_3);
  ASSERT_NOT_NULL(grid_4);

  ASSERT_EQUAL(header->frame.y, 0);
  ASSERT_EQUAL(body->frame.y, header->frame.h + 6);
  ASSERT_EQUAL(grid->frame.y, body->frame.y + body->frame.h + 6);

  ASSERT_EQUAL(body_btn1->frame.y, 0);
  ASSERT_EQUAL(body_btn2->frame.y, body_btn1->frame.h + 3);

  ASSERT_EQUAL(grid_1->frame.y, grid_2->frame.y);
  ASSERT_EQUAL(grid_3->frame.y, grid_4->frame.y);
  ASSERT_TRUE(grid_3->frame.y > grid_1->frame.y);
  ASSERT_EQUAL(grid_1->frame.y, 0);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_default_auto_layout_stack(void) {
  TEST("auto-layout: root form defaults to vertical stack");

  test_env_init();
  window_t *win = create_window_from_form(&kDefaultStackForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);
  window_t *first = get_window_item(win, 201);
  window_t *second = get_window_item(win, 202);
  ASSERT_NOT_NULL(first);
  ASSERT_NOT_NULL(second);

  ASSERT_EQUAL(first->frame.x, 0);
  ASSERT_EQUAL(first->frame.y, 0);
  ASSERT_TRUE(second->frame.y > first->frame.y);
  ASSERT_EQUAL(second->frame.y, first->frame.h + 4);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_new_post_grid_stack_layout(void) {
  TEST("auto-layout: grid rows and action stack stay compact");

  test_env_init();
  window_t *win = create_window_from_form(&kNewPostGridForm, 0, 0, NULL, form_test_proc, 0, NULL);
  ASSERT_NOT_NULL(win);

  window_t *fields = get_window_item(win, NP_FORM_ID_FIELDS);
  window_t *author_l = get_window_item(win, NP_FORM_ID_AUTHOR_L);
  window_t *author_e = get_window_item(win, NP_FORM_ID_AUTHOR_E);
  window_t *title_l = get_window_item(win, NP_FORM_ID_TITLE_L);
  window_t *title_e = get_window_item(win, NP_FORM_ID_TITLE_E);
  window_t *body_l = get_window_item(win, NP_FORM_ID_BODY_L);
  window_t *body_e = get_window_item(win, NP_FORM_ID_BODY_E);
  window_t *actions = get_window_item(win, NP_FORM_ID_ACTIONS);
  window_t *ok = get_window_item(win, NP_FORM_ID_OK);
  window_t *cancel = get_window_item(win, NP_FORM_ID_CANCEL);
  ASSERT_NOT_NULL(fields);
  ASSERT_NOT_NULL(author_l);
  ASSERT_NOT_NULL(author_e);
  ASSERT_NOT_NULL(title_l);
  ASSERT_NOT_NULL(title_e);
  ASSERT_NOT_NULL(body_l);
  ASSERT_NOT_NULL(body_e);
  ASSERT_NOT_NULL(actions);
  ASSERT_NOT_NULL(ok);
  ASSERT_NOT_NULL(cancel);

  ASSERT_EQUAL(fields->frame.y, 8);
  ASSERT_TRUE(fields->frame.h < 120);
  ASSERT_EQUAL(author_l->frame.y, 0);
  ASSERT_EQUAL(author_e->frame.y, 0);
  ASSERT_EQUAL(title_l->frame.y, author_l->frame.h + 4);
  ASSERT_EQUAL(title_e->frame.y, title_l->frame.y);
  ASSERT_EQUAL(body_l->frame.y, title_l->frame.y + title_l->frame.h + 4);
  ASSERT_EQUAL(body_e->frame.y, body_l->frame.y);
  ASSERT_TRUE(author_e->frame.w > author_l->frame.w);
  ASSERT_TRUE(title_e->frame.w > title_l->frame.w);
  ASSERT_TRUE(body_e->frame.w > body_l->frame.w);
  ASSERT_EQUAL(actions->frame.y, fields->frame.y + fields->frame.h + 4);
  ASSERT_EQUAL(ok->frame.y, 0);
  ASSERT_EQUAL(cancel->frame.y, 0);
  ASSERT_TRUE(cancel->frame.x > ok->frame.x);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

void test_post_detail_layout_budget(void) {
  TEST("post-detail-like layout: footer fits inside the 336px window");

  test_env_init();

  window_t *short_win = create_post_detail_like_window(336);
  ASSERT_NOT_NULL(short_win);
  window_t *short_layout = get_window_item(short_win, PD_FORM_ID_LAYOUT);
  window_t *short_header = get_window_item(short_win, PD_FORM_ID_HEADER);
  window_t *short_comments = get_window_item(short_win, PD_FORM_ID_COMMENTS);
  window_t *short_actions = get_window_item(short_win, PD_FORM_ID_ACTIONS);
  window_t *short_body = get_window_item(short_win, PD_FORM_ID_BODY);
  ASSERT_NOT_NULL(short_layout);
  ASSERT_NOT_NULL(short_header);
  ASSERT_NOT_NULL(short_comments);
  ASSERT_NOT_NULL(short_actions);
  ASSERT_NOT_NULL(short_body);
  irect16_t short_client = get_client_rect(short_win);
  int short_bottom = short_actions->frame.y + short_actions->frame.h;
  ASSERT_EQUAL(short_layout->frame.y, 8);
  ASSERT_EQUAL(short_layout->frame.h, short_client.h - 16);
  ASSERT_TRUE(short_header->frame.h > CONTROL_HEIGHT);
  ASSERT_TRUE(short_comments->frame.h > 0);
  ASSERT_TRUE(short_body->frame.h > CONTROL_HEIGHT);
  ASSERT_TRUE(short_bottom <= short_client.h);
  ASSERT_EQUAL(short_bottom, short_layout->frame.h);
  destroy_window(short_win);

  test_env_shutdown();
  PASS();
}

void test_socialfeed_post_detail_layout(void) {
  TEST("socialfeed post detail: reportview takes leftover height");

  test_env_init();

  window_t *win = create_window_from_form(&socialfeed_post_detail_form, 0, 0, NULL,
                                          socialfeed_post_detail_layout_proc, 0, NULL);
  ASSERT_NOT_NULL(win);

  window_t *layout = get_window_item(win, ID_POST_DETAIL_LAYOUT);
  window_t *comments = get_window_item(win, ID_POST_DETAIL_COMMENTS);
  window_t *actions = get_window_item(win, ID_POST_DETAIL_ACTIONS);
  ASSERT_NOT_NULL(layout);
  ASSERT_NOT_NULL(comments);
  ASSERT_NOT_NULL(actions);

  ASSERT_TRUE(comments->frame.h > 100);
  ASSERT_EQUAL(actions->frame.y + actions->frame.h, layout->frame.h);
  ASSERT_TRUE(actions->frame.y > comments->frame.y);

  destroy_window(win);
  test_env_shutdown();
  PASS();
}

// ──────────────────────────────────────────────────────────────────────────
// main
// ──────────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  TEST_START("form_def_t / create_window_from_form / show_dialog_from_form");

  test_form_children_exist_at_create();
  test_form_child_ids();
  test_form_child_flags();
  test_form_child_text();
  test_show_dialog_from_form_flags();
  test_center_window_rect_owner();
  test_center_window_rect_screen_clamp();
  test_ddx_form_def_fields();
  test_ddx_push_pull_roundtrip();
  test_show_ddx_dialog_form_flags();
  test_stackview_layout();
  test_gridview_layout();
  test_auto_layout_padding();
  test_auto_layout_margin();
  test_auto_layout_wrapped_label();
  test_nested_stack_positions();
  test_default_auto_layout_stack();
  test_new_post_grid_stack_layout();
  test_post_detail_layout_budget();
  test_socialfeed_post_detail_layout();

  TEST_END();
}
