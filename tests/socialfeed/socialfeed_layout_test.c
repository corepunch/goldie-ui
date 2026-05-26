// Test: Verify socialfeed dialog layouts - buttons should not expand vertically
//
// Issue: In post_detail and new_post dialogs, buttons were expanding equally 
// with flex content (tableview/multiedit), causing incorrect layout.
//
// Fix: Auto-add WINDOW_FLEXSPACE for <space> and <multiedit> elements in orionc,
// so only intended flex controls expand, leaving buttons at fixed height.

#include "ui.h"
#include "examples/socialfeed/socialfeed.h"
#include "test_framework.h"
#include "test_env.h"

// Simple window proc for testing
static lresult_t test_proc(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
    (void)win; (void)wparam; (void)lparam;
    if (msg == evCreate) return 1;
    return 0;
}

// ============================================================
// Test: Post detail dialog layout (tableview + buttons)
// ============================================================

static void test_post_detail_layout(void) {
    TEST("Post detail: tableview expands, buttons fixed height");
    
    // Create dialog (non-modal for testing) - skip database for this test
    window_t *dlg = create_window_from_form(&socialfeed_post_detail_form, 100, 100,
                                            NULL, test_proc, 0, NULL);
    ASSERT_NOT_NULL(dlg);
    
    // Force layout sync
    window_layout_sync(dlg);
    
    // Find children
    window_t *tableview = get_window_item(dlg, ID_POST_DETAIL_COMMENTS);
    window_t *author_col = get_window_item(dlg, ID_POST_DETAIL_COLUMN0);
    window_t *text_col = get_window_item(dlg, ID_POST_DETAIL_COLUMN1);
    window_t *likes_col = get_window_item(dlg, ID_POST_DETAIL_COLUMN2);
    window_t *actions = get_window_item(dlg, ID_POST_DETAIL_ACTIONS);
    
    ASSERT_NOT_NULL(tableview);
    ASSERT_NOT_NULL(author_col);
    ASSERT_NOT_NULL(text_col);
    ASSERT_NOT_NULL(likes_col);
    ASSERT_NOT_NULL(actions);
    
    // Verify tableview has WINDOW_FLEXSPACE
    ASSERT((tableview->flags & WINDOW_FLEXSPACE) != 0, "tableview should have WINDOW_FLEXSPACE");
    
    // Get client rect
    irect16_t cr = get_client_rect(dlg);
    
    // Verify tableview takes significant vertical space (at least 60% of client height)
    int min_tableview_height = (int)(cr.h * 0.6);
    ASSERT(tableview->frame.h >= min_tableview_height, "tableview should take at least 60% of vertical space");
    
    // Verify actions stack is at fixed height (approximately button height + spacing)
    // Standard button height is 19px, with spacing ~8px
    ASSERT(actions->frame.h >= 19, "actions height should be at least button height");
    ASSERT(actions->frame.h <= 35, "actions should not expand beyond button height + padding");
    
    // Verify actions is positioned below tableview (not overlapping)
    ASSERT(actions->frame.y > tableview->frame.y, "actions should be below tableview");
    ASSERT(actions->frame.y >= tableview->frame.y + tableview->frame.h, "actions should not overlap tableview");

    // Table columns are real child windows for FormEditor hit-testing, but
    // they share the report/table view's single scrolling surface.
    ASSERT(author_col->parent == tableview, "author column should be a tableview child");
    ASSERT(text_col->parent == tableview, "text column should be a tableview child");
    ASSERT(likes_col->parent == tableview, "likes column should be a tableview child");
    ASSERT(author_col->frame.x == 0, "first column should start at x=0");
    ASSERT(author_col->frame.w == 70, "author column should keep fixed width");
    ASSERT(likes_col->frame.w == 45, "likes column should keep fixed width");
    ASSERT(text_col->frame.x == author_col->frame.w, "text column should follow author column");
    ASSERT(likes_col->frame.x == text_col->frame.x + text_col->frame.w, "likes column should follow text column");
    ASSERT(author_col->frame.h == tableview->frame.h, "column height should match tableview");
    ASSERT(text_col->frame.h == tableview->frame.h, "column height should match tableview");
    ASSERT(likes_col->frame.h == tableview->frame.h, "column height should match tableview");
    
    // Cleanup
    destroy_window(dlg);
    
    PASS();
}

// ============================================================
// Test: New post dialog layout (multiedit + buttons)
// ============================================================

static void test_new_post_layout(void) {
    TEST("New post: multiedit expands, buttons fixed height");
    
    // Create dialog (non-modal for testing) - skip database for this test
    window_t *dlg = create_window_from_form(&socialfeed_new_post_form, 100, 100,
                                            NULL, test_proc, 0, NULL);
    ASSERT_NOT_NULL(dlg);
    
    // Force layout sync
    window_layout_sync(dlg);
    
    // Find children
    window_t *grid = get_window_item(dlg, ID_NEW_POST_FIELDS);
    window_t *multiedit = get_window_item(dlg, ID_NEW_POST_BODY);
    window_t *actions = get_window_item(dlg, ID_NEW_POST_ACTIONS);
    
    ASSERT_NOT_NULL(grid);
    ASSERT_NOT_NULL(multiedit);
    ASSERT_NOT_NULL(actions);
    
    // Verify multiedit has WINDOW_FLEXSPACE (should expand)
    ASSERT((multiedit->flags & WINDOW_FLEXSPACE) != 0, "multiedit should have WINDOW_FLEXSPACE");
    
    // Get client rect
    irect16_t cr = get_client_rect(dlg);
    
    // Verify multiedit takes significant vertical space (at least 40% of client height)
    int min_multiedit_height = (int)(cr.h * 0.4);
    ASSERT(multiedit->frame.h >= min_multiedit_height, "multiedit should take at least 40% of vertical space");
    
    // Verify actions stack is at fixed height (approximately button height + spacing)
    ASSERT(actions->frame.h >= 19, "actions height should be at least button height");
    ASSERT(actions->frame.h <= 35, "actions should not expand beyond button height + padding");
    
    // Verify actions is positioned below grid (not overlapping)
    ASSERT(actions->frame.y > grid->frame.y, "actions should be below grid");
    
    // Verify no overlap between multiedit and actions
    // multiedit is inside grid, so check grid bottom vs actions top
    ASSERT(actions->frame.y >= grid->frame.y + grid->frame.h, "actions should not overlap grid");
    
    // Cleanup
    destroy_window(dlg);
    
    PASS();
}

static void test_new_post_bindings_pull_database_record(void) {
    TEST("New post: generated bindings pull author, title, and body");

    database_t *db = create_database("socialfeed_ddx", "db_simple_xml", ":memory:");
    ASSERT_NOT_NULL(db);

    db_author_t author = {0};
    snprintf(author.name, sizeof(author.name), "%s", "frank");
    snprintf(author.avatar, sizeof(author.avatar), "%s", "frank.png");
    db_author_t *inserted_author = (db_author_t *)send_db_message(db, dbInsert, ID_DB_AUTHORS, &author);
    ASSERT_NOT_NULL(inserted_author);
    snprintf(inserted_author->name, sizeof(inserted_author->name),
             "frank-ddx-%d", inserted_author->id);
    ASSERT_TRUE(send_db_message(db, dbUpdate, ID_DB_AUTHORS, inserted_author));

    ui_set_database(db);
    ASSERT_TRUE(register_database("db", db));

    ASSERT_EQUAL(socialfeed_new_post_bindings[0].getter, cbGetCurrentValue);
    ASSERT_EQUAL(socialfeed_new_post_bindings[1].getter, edGetText);
    ASSERT_EQUAL(socialfeed_new_post_bindings[2].getter, edGetText);

    window_t *dlg = create_window_from_form(&socialfeed_new_post_form, 100, 100,
                                            NULL, test_proc, 0, NULL);
    ASSERT_NOT_NULL(dlg);

    window_t *author_combo = get_window_item(dlg, ID_NEW_POST_AUTHOR);
    window_t *title = get_window_item(dlg, ID_NEW_POST_TITLE);
    window_t *body = get_window_item(dlg, ID_NEW_POST_BODY);
    ASSERT_NOT_NULL(author_combo);
    ASSERT_NOT_NULL(title);
    ASSERT_NOT_NULL(body);

    ASSERT_TRUE(author_combo->cursor_pos > 0);
    send_message(author_combo, cbSetCurrentSelection, author_combo->cursor_pos - 1, NULL);
    send_message(title, edSetText, 0, "Generated DDX works");
    send_message(body, edSetText, 0, "The multiline body is copied into the post record.");

    db_post_t post = {0};
    dialog_pull(dlg, &post,
                socialfeed_new_post_form.bindings,
                socialfeed_new_post_form.binding_count);

    ASSERT_EQUAL(post.author_id, inserted_author->id);
    ASSERT_STR_EQUAL(post.title, "Generated DDX works");
    ASSERT_STR_EQUAL(post.body, "The multiline body is copied into the post record.");

    db_post_t *inserted_post = (db_post_t *)send_db_message(db, dbInsert, ID_DB_POSTS, &post);
    ASSERT_NOT_NULL(inserted_post);
    ASSERT_STR_EQUAL(inserted_post->title, "Generated DDX works");

    destroy_window(dlg);
    ui_set_database(NULL);
    db->dirty = false;
    destroy_database(db);

    PASS();
}

// ============================================================
// Test: Space elements get WINDOW_FLEXSPACE automatically
// ============================================================

static void test_space_element_flexspace(void) {
    TEST("Space elements automatically get WINDOW_FLEXSPACE from orionc");
    
    // Create dialog
    window_t *dlg = create_window_from_form(&socialfeed_post_detail_form, 100, 100,
                                            NULL, test_proc, 0, NULL);
    ASSERT_NOT_NULL(dlg);
    
    // Find space element inside actions stack
    window_t *space = get_window_item(dlg, ID_POST_DETAIL_FLEX);
    ASSERT_NOT_NULL(space);
    
    // Verify space has WINDOW_FLEXSPACE (auto-added by orionc)
    ASSERT((space->flags & WINDOW_FLEXSPACE) != 0, "space element should have WINDOW_FLEXSPACE");
    
    // Cleanup
    destroy_window(dlg);
    
    PASS();
}

// ============================================================
// Main
// ============================================================

int main(void) {
    TEST_START("socialfeed layout");
    
    // Initialize UI system
    test_env_init();
    
    // Register database class
    DB_CLASS(db_simple_xml);
    
    test_post_detail_layout();
    test_new_post_layout();
    test_new_post_bindings_pull_database_record();
    test_space_element_flexspace();
    
    test_env_shutdown();
    
    TEST_END();
}
