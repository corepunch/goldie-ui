// Headless smoke tests for the current window-first FormEditor runtime.

#include "test_framework.h"
#include "test_env.h"
#include "examples/formeditor/formeditor.h"
#include "commctl/commctl.h"

#include <stdlib.h>
#include <string.h>

app_state_t *g_app = NULL;

int message_box(window_t *parent, const char *text,
                const char *caption, uint32_t type) {
  (void)parent;
  (void)text;
  (void)caption;
  (void)type;
  return IDCANCEL;
}

static void fe_test_close_all_docs(void) {
  if (!g_app)
    return;
  while (g_app->form_count > 0 && g_app->forms[0])
    close_form_doc(g_app->forms[0]);
}

static void fe_setup(void) {
  if (g_app) {
    fe_test_close_all_docs();
    for (int i = 0; i < FE_NUM_WINDOWS; i++) {
      if (g_app->windows[i] && is_window(g_app->windows[i]))
        destroy_window(g_app->windows[i]);
      g_app->windows[i] = NULL;
    }
    free(g_app);
    g_app = NULL;
  }

  test_env_init();
  g_app = calloc(1, sizeof(app_state_t));
  if (!g_app)
    return;
  g_app->current_tool = ID_TOOL_SELECT;
  (void)create_form_doc(FORM_DEFAULT_W, FORM_DEFAULT_H);
}

static void fe_teardown(void) {
  if (!g_app) {
    test_env_shutdown();
    return;
  }

  fe_test_close_all_docs();
  for (int i = 0; i < FE_NUM_WINDOWS; i++) {
    if (g_app->windows[i] && is_window(g_app->windows[i]))
      destroy_window(g_app->windows[i]);
    g_app->windows[i] = NULL;
  }
  free(g_app);
  g_app = NULL;
  test_env_shutdown();
}

static window_t *fe_active_doc(void) {
  return g_app ? g_app->active_form : NULL;
}

static int fe_component_id_for_class_name(const char *class_name) {
  const fe_component_desc_t *desc = fe_component_by_class_name(class_name);
  if (!desc)
    return -1;
  for (int i = 0; i < fe_component_count(); i++) {
    if (fe_component_at(i) == desc)
      return i;
  }
  return -1;
}

static int fe_child_count(window_t *parent) {
  int count = 0;
  for (window_t *child = parent ? parent->children : NULL; child; child = child->next)
    count++;
  return count;
}

static window_t *fe_find_child_by_title(window_t *parent, const char *title) {
  for (window_t *child = parent ? parent->children : NULL; child; child = child->next) {
    if (strcmp(child->title, title) == 0)
      return child;
  }
  return NULL;
}

static int fe_child_count_by_proc(window_t *parent, winproc_t proc) {
  int count = 0;
  for (window_t *child = parent ? parent->children : NULL; child; child = child->next) {
    if (child->proc == proc)
      count++;
  }
  return count;
}

static window_t *fe_child_at_by_proc(window_t *parent, winproc_t proc, int index) {
  int i = 0;
  for (window_t *child = parent ? parent->children : NULL; child; child = child->next) {
    if (child->proc != proc)
      continue;
    if (i == index)
      return child;
    i++;
  }
  return NULL;
}

static window_t *fe_find_root_by_title(const char *title) {
  for (window_t *win = g_ui_runtime.windows; win; win = win->next) {
    if (strcmp(win->title, title) == 0)
      return win;
  }
  return NULL;
}

static window_t *fe_browser_list(window_t *browser) {
  return browser ? browser->children : NULL;
}

static window_t *fe_paint_probe_browser = NULL;
static int fe_paint_probe_count = 0;
static int fe_paint_probe_bad_scroll_count = 0;
static int fe_test_db_fetch_count = 0;

static void fe_columnbrowser_paint_probe(window_t *win, uint32_t msg, uint32_t wparam,
                                         void *lparam, void *userdata) {
  (void)msg; (void)wparam; (void)lparam; (void)userdata;
  if (!fe_paint_probe_browser || !win || win->parent != fe_paint_probe_browser ||
      win->proc != win_reportview)
    return;
  fe_paint_probe_count++;
  if (fe_paint_probe_browser->hscroll.pos != 0)
    fe_paint_probe_bad_scroll_count++;
}

typedef struct {
  int id;
  char title[64];
  int author_id;
} fe_test_post_t;

typedef struct {
  int id;
  char name[64];
} fe_test_author_t;

static const db_field_schema_t fe_test_post_fields[] = {
  { .field_id = 1, .name = "id", .type = DB_TYPE_INT, .primary_key = true },
  { .field_id = 2, .name = "title", .type = DB_TYPE_STRING, .length = 64 },
  { .field_id = 3, .name = "author_id", .type = DB_TYPE_INT, .relation_table_id = 2, .relation_field_id = 4 },
};

static const db_field_schema_t fe_test_author_fields[] = {
  { .field_id = 4, .name = "id", .type = DB_TYPE_INT, .primary_key = true },
  { .field_id = 5, .name = "name", .type = DB_TYPE_STRING, .length = 64 },
};

static const db_field_schema_t fe_test_comment_fields[] = {
  { .field_id = 6, .name = "id", .type = DB_TYPE_INT, .primary_key = true },
  { .field_id = 7, .name = "post_id", .type = DB_TYPE_INT, .relation_table_id = 1, .relation_field_id = 1 },
  { .field_id = 8, .name = "text", .type = DB_TYPE_STRING, .length = 256 },
};

static const db_field_meta_t fe_test_author_meta[] = {
  { 4, "id", DB_TYPE_INT, offsetof(fe_test_author_t, id), 0 },
  { 5, "name", DB_TYPE_STRING, offsetof(fe_test_author_t, name), 64 },
};

static const db_join_schema_t fe_test_post_joins[] = {
  { 3, "author", 3, 2, 4 },
};

static const db_join_schema_t fe_test_comment_joins[] = {
  { 7, "post", 7, 1, 1 },
};

static fe_test_author_t fe_test_authors[] = {
  { 1, "alice" },
  { 2, "bob" },
};

static const db_table_schema_t fe_test_db_tables[] = {
  {
    .table_id = 1,
    .name = "posts",
    .fields = fe_test_post_fields,
    .field_count = ARRAY_LEN(fe_test_post_fields),
    .joins = fe_test_post_joins,
    .join_count = ARRAY_LEN(fe_test_post_joins),
  },
  {
    .table_id = 2,
    .name = "authors",
    .fields = fe_test_author_fields,
    .field_count = ARRAY_LEN(fe_test_author_fields),
  },
  {
    .table_id = 3,
    .name = "comments",
    .fields = fe_test_comment_fields,
    .field_count = ARRAY_LEN(fe_test_comment_fields),
    .joins = fe_test_comment_joins,
    .join_count = ARRAY_LEN(fe_test_comment_joins),
  },
};

static const db_schema_def_t fe_test_db_schema = {
  .name = "db",
  .class_name = "test",
  .tables = fe_test_db_tables,
  .table_count = ARRAY_LEN(fe_test_db_tables),
};

static lresult_t fe_test_db_proc(database_t *db, uint32_t msg, uint32_t wparam, void *lparam) {
  (void)db;
  if (msg == dbGetSchema)
    return (lresult_t)&fe_test_db_schema;
  if (msg == dbGetFieldMeta && wparam == 2) {
    if (lparam)
      *(int *)lparam = ARRAY_LEN(fe_test_author_meta);
    return (lresult_t)fe_test_author_meta;
  }
  if (msg == dbFetch && LOWORD(wparam) == 2) {
    fe_test_db_fetch_count++;
    result_node_t *head = NULL;
    result_node_t *tail = NULL;
    for (int i = 0; i < (int)ARRAY_LEN(fe_test_authors); i++) {
      result_node_t *node = malloc(sizeof(result_node_t) + sizeof(void *));
      if (!node) {
        free_result_list(head);
        return 0;
      }
      node->next = NULL;
      *(void **)node->data = &fe_test_authors[i];
      if (tail) tail->next = node;
      else head = node;
      tail = node;
    }
    return (lresult_t)head;
  }
  if (msg == dbFetch) {
    fe_test_db_fetch_count++;
    return 0;
  }
  return true;
}

void test_fe_create_doc_tracks_active_form(void) {
  TEST("create_form_doc: tracks active window-first document state");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *doc = fe_active_doc();

  ASSERT_NOT_NULL(doc);
  ASSERT_TRUE(is_window(doc));
  ASSERT_EQUAL(g_app->form_count, 1);
  ASSERT_TRUE(g_app->forms[0] == doc);
  ASSERT_NOT_NULL(fe_doc_state(doc));
  ASSERT_FALSE(fe_doc_state(doc)->modified);
  ASSERT_STR_EQUAL(doc->title, "Untitled");
  ASSERT_NOT_NULL(doc->children);
  ASSERT_TRUE(doc->children->parent == doc);
  ASSERT_TRUE(window_has_state(doc, WINDOW_STATE_VISIBLE));

  fe_teardown();
  PASS();
}

void test_fe_multiple_docs_and_activation(void) {
  TEST("document lifecycle: create, activate, hide others, and close");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *first = fe_active_doc();
  window_t *second = create_form_doc(400, 260);

  ASSERT_NOT_NULL(first);
  ASSERT_NOT_NULL(second);
  ASSERT_EQUAL(g_app->form_count, 2);
  ASSERT_TRUE(fe_active_doc() == second);

  form_doc_show_only(first);
  ASSERT_TRUE(fe_active_doc() == first);
  ASSERT_TRUE(window_has_state(first, WINDOW_STATE_VISIBLE));
  ASSERT_FALSE(window_has_state(second, WINDOW_STATE_VISIBLE));

  close_form_doc(first);
  ASSERT_EQUAL(g_app->form_count, 1);
  ASSERT_TRUE(fe_active_doc() == second);
  ASSERT_TRUE(is_window(second));

  fe_teardown();
  PASS();
}

void test_fe_close_message_hides_but_does_not_destroy(void) {
  TEST("document evClose: hides editor window without destroying it");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *doc = fe_active_doc();

  ASSERT_TRUE(send_message(doc, evClose, 0, NULL));
  ASSERT_TRUE(is_window(doc));
  ASSERT_EQUAL(g_app->form_count, 1);
  ASSERT_FALSE(window_has_state(doc, WINDOW_STATE_VISIBLE));

  fe_teardown();
  PASS();
}

void test_fe_component_registry_has_core_controls(void) {
  TEST("component registry: core controls and layout containers are registered");

  fe_setup();
  ASSERT_NOT_NULL(g_app);

  ASSERT_TRUE(fe_component_id_for_class_name("Button") >= 0);
  ASSERT_TRUE(fe_component_id_for_class_name("CheckBox") >= 0);
  ASSERT_TRUE(fe_component_id_for_class_name("GridView") >= 0);
  ASSERT_TRUE(fe_component_id_for_class_name("Column") >= 0);

  fe_teardown();
  PASS();
}

void test_fe_drop_component_to_document_marks_modified(void) {
  TEST("component drop: dropping onto document creates a live child and marks modified");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *doc = fe_active_doc();
  int button_type = fe_component_id_for_class_name("Button");
  int before = fe_child_count(doc);
  ipoint16_t origin = window_client_origin_xy(doc);

  ASSERT_TRUE(button_type >= 0);
  ASSERT_TRUE(canvas_drop_component_to_target(doc, button_type, doc, origin.x + 24, origin.y + 24));
  ASSERT_EQUAL(fe_child_count(doc), before + 1);
  ASSERT_NOT_NULL(fe_find_child_by_title(doc, "Button"));
  ASSERT_TRUE(fe_doc_state(doc)->modified);
  ASSERT_TRUE(g_app->project.modified);

  fe_teardown();
  PASS();
}

void test_fe_drop_component_into_layout_container(void) {
  TEST("component drop: explicit layout container target becomes the parent");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *doc = fe_active_doc();
  int grid_type = fe_component_id_for_class_name("GridView");
  int button_type = fe_component_id_for_class_name("Button");
  ipoint16_t origin = window_client_origin_xy(doc);

  ASSERT_TRUE(grid_type >= 0);
  ASSERT_TRUE(button_type >= 0);
  ASSERT_TRUE(canvas_drop_component_to_target(doc, grid_type, doc, origin.x + 20, origin.y + 20));

  window_t *grid = fe_find_child_by_title(doc, "GridView");
  ASSERT_NOT_NULL(grid);
  ASSERT_TRUE(canvas_drop_component_to_target(doc, button_type, grid, origin.x + 30, origin.y + 30));

  window_t *button = fe_find_child_by_title(grid, "Button");
  ASSERT_NOT_NULL(button);
  ASSERT_TRUE(button->parent == grid);

  fe_teardown();
  PASS();
}

void test_fe_forms_browser_lists_open_documents(void) {
  TEST("forms browser: refresh lists the currently open documents");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *browser = forms_browser_create(0);
  ASSERT_NOT_NULL(browser);
  g_app->windows[FE_WIN_FORMS] = browser;

  ASSERT_NOT_NULL(create_form_doc(420, 260));
  forms_browser_refresh();

  window_t *list = fe_browser_list(browser);
  ASSERT_NOT_NULL(list);
  ASSERT_EQUAL((int)send_message(list, RVM_GETITEMCOUNT, 0, NULL), g_app->form_count);

  fe_teardown();
  PASS();
}

void test_fe_property_browser_refresh_populates_rows(void) {
  TEST("property browser: refresh populates reportview rows for the active document");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *browser = property_browser_create(0);
  ASSERT_NOT_NULL(browser);
  g_app->windows[FE_WIN_PROP] = browser;

  property_browser_refresh(fe_active_doc());

  window_t *list = fe_browser_list(browser);
  ASSERT_NOT_NULL(list);
  ASSERT_TRUE((int)send_message(list, RVM_GETITEMCOUNT, 0, NULL) > 0);

  fe_teardown();
  PASS();
}

void test_fe_plugins_browser_lists_project_plugins(void) {
  TEST("plugins browser: refresh reflects project plugin references");

  fe_setup();
  ASSERT_NOT_NULL(g_app);
  window_t *browser = plugins_browser_create(0);
  ASSERT_NOT_NULL(browser);
  g_app->windows[FE_WIN_PLUGINS] = browser;

  snprintf(g_app->project.plugins[0].name,
           sizeof(g_app->project.plugins[0].name),
           "%s", "example-plugin.dylib");
  g_app->project.plugin_count = 1;
  plugins_browser_refresh();

  window_t *list = fe_browser_list(browser);
  ASSERT_NOT_NULL(list);
  ASSERT_EQUAL((int)send_message(list, RVM_GETITEMCOUNT, 0, NULL), 1);

  fe_teardown();
  PASS();
}

void test_fe_database_browser_cascades_reportviews(void) {
  TEST("database browser: selection spawns independent reportview columns");

  fe_setup();
  ASSERT_NOT_NULL(g_app);

  database_t db = {
    .name = "db",
    .class_name = "test",
    .proc = fe_test_db_proc,
  };
  g_app->project.databases[0] = &db;
  g_app->project.database_count = 1;
  fe_test_db_fetch_count = 0;

  formeditor_show_database_object_window(0);

  window_t *browser = fe_find_root_by_title("Databases");
  ASSERT_NOT_NULL(browser);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 1);

  window_t *db_list = fe_child_at_by_proc(browser, win_reportview, 0);
  ASSERT_NOT_NULL(db_list);
  ASSERT_EQUAL((int)send_message(db_list, RVM_GETITEMCOUNT, 0, NULL), 1);
  reportview_item_t db_item = {0};
  ASSERT_TRUE(send_message(db_list, RVM_GETITEMDATA, 0, &db_item));
  ASSERT_TRUE((db_item.flags & RVI_DISCLOSURE) != 0);

  send_message(db_list, RVM_SETSELECTION, 0, NULL);
  send_message(browser, evCommand, MAKEDWORD(0, RVN_SELCHANGE), db_list);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 2);

  window_t *table_list = fe_child_at_by_proc(browser, win_reportview, 1);
  ASSERT_NOT_NULL(table_list);
  ASSERT_EQUAL((int)send_message(table_list, RVM_GETITEMCOUNT, 0, NULL),
               (int)ARRAY_LEN(fe_test_db_tables));
  reportview_item_t table_item = {0};
  ASSERT_TRUE(send_message(table_list, RVM_GETITEMDATA, 0, &table_item));
  ASSERT_TRUE((table_item.flags & RVI_DISCLOSURE) != 0);

  send_message(table_list, RVM_SETSELECTION, 0, NULL);
  send_message(browser, evCommand, MAKEDWORD(0, RVN_SELCHANGE), table_list);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 3);

  window_t *field_list = fe_child_at_by_proc(browser, win_reportview, 2);
  ASSERT_NOT_NULL(field_list);
  ASSERT_EQUAL((int)send_message(field_list, RVM_GETITEMCOUNT, 0, NULL),
               (int)ARRAY_LEN(fe_test_post_fields));
  reportview_item_t field_item = {0};
  ASSERT_TRUE(send_message(field_list, RVM_GETITEMDATA, 0, &field_item));
  ASSERT_FALSE((field_item.flags & RVI_DISCLOSURE) != 0);

  reportview_item_t relation_item = {0};
  ASSERT_TRUE(send_message(field_list, RVM_GETITEMDATA, 2, &relation_item));
  ASSERT_TRUE((relation_item.flags & RVI_DISCLOSURE) != 0);

  send_message(field_list, RVM_SETSELECTION, 2, NULL);
  send_message(browser, evCommand, MAKEDWORD(2, RVN_SELCHANGE), field_list);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 4);

  window_t *related_field_list = fe_child_at_by_proc(browser, win_reportview, 3);
  ASSERT_NOT_NULL(related_field_list);
  ASSERT_EQUAL((int)send_message(related_field_list, RVM_GETITEMCOUNT, 0, NULL),
               (int)ARRAY_LEN(fe_test_author_fields));
  reportview_item_t related_field_item = {0};
  ASSERT_TRUE(send_message(related_field_list, RVM_GETITEMDATA, 0, &related_field_item));
  ASSERT_STR_EQUAL(related_field_item.text, "id");

  send_message(table_list, RVM_SETSELECTION, 2, NULL);
  send_message(browser, evCommand, MAKEDWORD(2, RVN_SELCHANGE), table_list);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 3);

  field_list = fe_child_at_by_proc(browser, win_reportview, 2);
  ASSERT_NOT_NULL(field_list);
  ASSERT_EQUAL((int)send_message(field_list, RVM_GETITEMCOUNT, 0, NULL),
               (int)ARRAY_LEN(fe_test_comment_fields));
  reportview_item_t comment_relation_item = {0};
  ASSERT_TRUE(send_message(field_list, RVM_GETITEMDATA, 1, &comment_relation_item));
  ASSERT_STR_EQUAL(comment_relation_item.text, "post");
  ASSERT_TRUE((comment_relation_item.flags & RVI_DISCLOSURE) != 0);

  send_message(field_list, RVM_SETSELECTION, 1, NULL);
  send_message(browser, evCommand, MAKEDWORD(1, RVN_SELCHANGE), field_list);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 4);

  window_t *post_field_list = fe_child_at_by_proc(browser, win_reportview, 3);
  ASSERT_NOT_NULL(post_field_list);
  ASSERT_EQUAL((int)send_message(post_field_list, RVM_GETITEMCOUNT, 0, NULL),
               (int)ARRAY_LEN(fe_test_post_fields));
  reportview_item_t post_relation_item = {0};
  ASSERT_TRUE(send_message(post_field_list, RVM_GETITEMDATA, 2, &post_relation_item));
  ASSERT_STR_EQUAL(post_relation_item.text, "author");
  ASSERT_TRUE((post_relation_item.flags & RVI_DISCLOSURE) != 0);

  send_message(post_field_list, RVM_SETSELECTION, 2, NULL);
  send_message(browser, evCommand, MAKEDWORD(2, RVN_SELCHANGE), post_field_list);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 5);

  window_t *author_field_list = fe_child_at_by_proc(browser, win_reportview, 4);
  ASSERT_NOT_NULL(author_field_list);
  ASSERT_EQUAL((int)send_message(author_field_list, RVM_GETITEMCOUNT, 0, NULL),
               (int)ARRAY_LEN(fe_test_author_fields));
  reportview_item_t author_field_item = {0};
  ASSERT_TRUE(send_message(author_field_list, RVM_GETITEMDATA, 0, &author_field_item));
  ASSERT_STR_EQUAL(author_field_item.text, "id");
  ASSERT_EQUAL(fe_test_db_fetch_count, 0);
  ASSERT_TRUE(browser->hscroll.visible);

  irect16_t browser_cr = get_client_rect(browser);
  int hscroll_x = browser->frame.x + browser_cr.w - 2;
  int hscroll_y = browser->frame.y + titlebar_height(browser) + browser_cr.h + 1;
  window_t *hscroll_hit = find_window(hscroll_x, hscroll_y);
  ASSERT_TRUE(hscroll_hit == browser);
  ui_event_t hscroll_down = {
    .message = kEventLeftButtonDown,
    .x = hscroll_x * UI_WINDOW_SCALE,
    .y = hscroll_y * UI_WINDOW_SCALE,
  };
  dispatch_message(&hscroll_down);
  ASSERT_TRUE((int)browser->hscroll.pos > 0);

  send_message(browser, evHScroll, 0, NULL);
  ASSERT_EQUAL((int)browser->hscroll.pos, 0);

  browser_cr = get_client_rect(browser);
  hscroll_x = browser->frame.x + SCROLLBAR_WIDTH + 2;
  hscroll_y = browser->frame.y + titlebar_height(browser) + browser_cr.h + 1;
  ASSERT_TRUE(find_window(hscroll_x, hscroll_y) == browser);
  hscroll_down.x = hscroll_x * UI_WINDOW_SCALE;
  hscroll_down.y = hscroll_y * UI_WINDOW_SCALE;
  dispatch_message(&hscroll_down);
  ASSERT_TRUE(browser->hscroll.dragging);

  ui_event_t hscroll_drag = {
    .message = kEventLeftButtonDragged,
    .x = (hscroll_x + 12) * UI_WINDOW_SCALE,
    .y = hscroll_y * UI_WINDOW_SCALE,
    .dx = 12,
    .dy = 0,
  };
  dispatch_message(&hscroll_drag);
  ASSERT_TRUE((int)browser->hscroll.pos > 0);
  ui_event_t hscroll_up = {
    .message = kEventLeftButtonUp,
    .x = (hscroll_x + 12) * UI_WINDOW_SCALE,
    .y = hscroll_y * UI_WINDOW_SCALE,
  };
  dispatch_message(&hscroll_up);
  ASSERT_FALSE(browser->hscroll.dragging);
  ASSERT_EQUAL(db_list->frame.h, get_client_rect(browser).h);

  int max_hscroll = browser->hscroll.max_val - (int)browser->hscroll.page;
  if (max_hscroll < 0)
    max_hscroll = 0;
  if (max_hscroll > 1) {
    int middle_hscroll = max_hscroll / 2;
    send_message(browser, evHScroll, (uint32_t)middle_hscroll, NULL);
    hscroll_x = browser->frame.x + browser_cr.w - 2;
    hscroll_y = browser->frame.y + titlebar_height(browser) + browser_cr.h + 1;
    hscroll_down.x = hscroll_x * UI_WINDOW_SCALE;
    hscroll_down.y = hscroll_y * UI_WINDOW_SCALE;
    dispatch_message(&hscroll_down);
    ASSERT_TRUE((int)browser->hscroll.pos > middle_hscroll);
  }

  send_message(browser, evHScroll, (uint32_t)max_hscroll, NULL);
  int field_hit_x = browser->frame.x + author_field_list->frame.x + 10;
  int field_hit_y = browser->frame.y + titlebar_height(browser) + 10;
  ASSERT_TRUE(find_window(field_hit_x, field_hit_y) == author_field_list);

  fe_paint_probe_browser = browser;
  fe_paint_probe_count = 0;
  fe_paint_probe_bad_scroll_count = 0;
  register_window_hook(evPaint, fe_columnbrowser_paint_probe, NULL);
  send_message(browser, evPaint, 0, NULL);
  deregister_window_hook(evPaint, fe_columnbrowser_paint_probe, NULL);
  fe_paint_probe_browser = NULL;
  ASSERT_TRUE(fe_paint_probe_count > 0);
  ASSERT_EQUAL(fe_paint_probe_bad_scroll_count, 0);

  send_message(db_list, RVM_SETSELECTION, 0, NULL);
  send_message(browser, evCommand, MAKEDWORD(0, RVN_SELCHANGE), db_list);
  ASSERT_EQUAL(fe_child_count_by_proc(browser, win_reportview), 2);

  fe_teardown();
  PASS();
}

int main(void) {
  TEST_START("Form Editor Window-First Smoke");

  test_fe_create_doc_tracks_active_form();
  test_fe_multiple_docs_and_activation();
  test_fe_close_message_hides_but_does_not_destroy();
  test_fe_component_registry_has_core_controls();
  test_fe_drop_component_to_document_marks_modified();
  test_fe_drop_component_into_layout_container();
  test_fe_forms_browser_lists_open_documents();
  test_fe_property_browser_refresh_populates_rows();
  test_fe_plugins_browser_lists_project_plugins();
  test_fe_database_browser_cascades_reportviews();

  TEST_END();
}
