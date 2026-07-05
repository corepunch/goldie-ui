// Current gitclient integration tests: generated Orion resources, database
// loading, and controller operations against a real temporary repository.

#include "test_framework.h"
#include "test_env.h"
#include "gitclient_test_helpers.h"
#include "examples/gitclient/gitclient.h"

#include <stdio.h>
#include <string.h>

static gc_state_t g_test_state;
gc_state_t *g_gc = &g_test_state;
static char s_repo[256];

static const form_ctrl_def_t *generated_control(uint32_t id) {
    for (int i = 0; i < gc_main_window_form.child_count; i++)
        if (gc_main_window_form.children[i].id == id)
            return &gc_main_window_form.children[i];
    return NULL;
}

static int result_count(result_node_t *rows) {
    int count = 0;
    for (; rows; rows = rows->next) count++;
    return count;
}

static bool setup_repo(void) {
    if (!gct_make_temp_dir(s_repo, sizeof(s_repo), "orion_gcui")) return false;
    if (!gct_git(s_repo, "init -b main") && !gct_git(s_repo, "init")) return false;
    if (!gct_git(s_repo, "config user.email ci@test") ||
        !gct_git(s_repo, "config user.name CI")) return false;
    char path[512]; snprintf(path, sizeof(path), "%s/file.txt", s_repo);
    return gct_write_file(path, "one\n") && gct_git(s_repo, "add file.txt") &&
           gct_git(s_repo, "commit -m initial");
}

static void setup_state(void) {
    test_env_init();
    memset(g_gc, 0, sizeof(*g_gc));
    DB_CLASS(gitclient_db);
    g_gc->db = create_database("gitclient-test", "gitclient_db", NULL);
    g_gc->repo = git_repo_open(s_repo);
}

static void teardown_state(void) {
    git_repo_close(g_gc->repo); g_gc->repo = NULL;
    destroy_database(g_gc->db); g_gc->db = NULL;
    test_env_shutdown();
}

void test_generated_context_menus_attach_to_expected_controls(void) {
    TEST("gitclient Orion: generated context menus attach to all four tables");
    const form_ctrl_def_t *branches = generated_control(ID_MAIN_WINDOW_BRANCHES);
    const form_ctrl_def_t *tags     = generated_control(ID_MAIN_WINDOW_TAGS);
    const form_ctrl_def_t *stash    = generated_control(ID_MAIN_WINDOW_STASH_LIST);
    const form_ctrl_def_t *files    = generated_control(ID_MAIN_WINDOW_FILES);
    ASSERT_NOT_NULL(branches); ASSERT_NOT_NULL(tags); ASSERT_NOT_NULL(stash); ASSERT_NOT_NULL(files);
    ASSERT_TRUE(branches->context_menu == CONTEXT_MENU_BRANCHES_ITEMS);
    ASSERT_TRUE(tags->context_menu     == CONTEXT_MENU_TAGS_ITEMS);
    ASSERT_TRUE(stash->context_menu    == CONTEXT_MENU_STASH_ITEMS);
    ASSERT_TRUE(files->context_menu    == CONTEXT_MENU_FILES_ITEMS);
    ASSERT_EQUAL(branches->context_menu_count, 4);
    ASSERT_EQUAL(tags->context_menu_count, 1);
    ASSERT_EQUAL(stash->context_menu_count, 2);
    ASSERT_EQUAL(files->context_menu_count, 4);
    PASS();
}

void test_generated_context_menus_reuse_shared_commands(void) {
    TEST("gitclient Orion: context entries reuse normal command IDs");
    ASSERT_EQUAL(CONTEXT_MENU_BRANCHES_ITEMS[0].id, ID_BRANCH_CHECKOUT);
    ASSERT_EQUAL(CONTEXT_MENU_BRANCHES_ITEMS[1].id, ID_BRANCH_MERGE);
    ASSERT_EQUAL(CONTEXT_MENU_BRANCHES_ITEMS[3].id, ID_BRANCH_DELETE);
    ASSERT_EQUAL(CONTEXT_MENU_TAGS_ITEMS[0].id, ID_TAG_DELETE);
    ASSERT_EQUAL(CONTEXT_MENU_STASH_ITEMS[0].id, ID_COMMIT_STASH_POP);
    ASSERT_EQUAL(CONTEXT_MENU_STASH_ITEMS[1].id, ID_STASH_DROP);
    ASSERT_EQUAL(CONTEXT_MENU_FILES_ITEMS[0].id, ID_FILES_STAGE);
    ASSERT_EQUAL(CONTEXT_MENU_FILES_ITEMS[1].id, ID_FILES_UNSTAGE);
    ASSERT_EQUAL(CONTEXT_MENU_FILES_ITEMS[3].id, ID_FILES_DISCARD);
    PASS();
}

void test_tag_controller_round_trip_and_database_load(void) {
    TEST("gitclient tags: create, load into database, and delete");
    setup_state();
    ASSERT_TRUE(gc_create_tag("release-test", "HEAD"));
    gc_load_tags();
    result_node_t *rows = (result_node_t *)send_db_message(
        g_gc->db, dbFetch, MAKEDWORD(ID_DB_TAGS, 0), NULL);
    ASSERT_EQUAL(result_count(rows), 1);
    db_tag_t *tag = rows ? *(db_tag_t **)rows->data : NULL;
    ASSERT_NOT_NULL(tag);
    ASSERT_STR_EQUAL(tag->name, "release-test");
    ASSERT_TRUE(strlen(tag->hash) == 40);
    free_result_list(rows);
    ASSERT_TRUE(gc_delete_tag("release-test"));
    gc_load_tags();
    rows = (result_node_t *)send_db_message(g_gc->db, dbFetch,
                                             MAKEDWORD(ID_DB_TAGS, 0), NULL);
    ASSERT_NULL(rows);
    teardown_state();
    PASS();
}

void test_stash_controller_load_and_drop(void) {
    TEST("gitclient stash: load database rows and drop selected ref");
    char path[512]; snprintf(path, sizeof(path), "%s/file.txt", s_repo);
    ASSERT_TRUE(gct_append_file(path, "two\n"));
    ASSERT_TRUE(gct_git(s_repo, "stash push -m ui-test"));
    setup_state();
    gc_load_stash();
    result_node_t *rows = (result_node_t *)send_db_message(
        g_gc->db, dbFetch, MAKEDWORD(ID_DB_STASH, 0), NULL);
    ASSERT_EQUAL(result_count(rows), 1);
    db_stash_t *stash = rows ? *(db_stash_t **)rows->data : NULL;
    ASSERT_NOT_NULL(stash);
    ASSERT_STR_EQUAL(stash->ref, "stash@{0}");
    ASSERT_TRUE(strstr(stash->message, "ui-test") != NULL);
    free_result_list(rows);
    ASSERT_TRUE(gc_stash_drop("stash@{0}"));
    gc_load_stash();
    rows = (result_node_t *)send_db_message(g_gc->db, dbFetch,
                                             MAKEDWORD(ID_DB_STASH, 0), NULL);
    ASSERT_NULL(rows);
    teardown_state();
    PASS();
}

int main(void) {
    if (!setup_repo()) { printf("ERROR: could not create test repository\n"); return 1; }
    TEST_START("Git Client Current Architecture");
    test_generated_context_menus_attach_to_expected_controls();
    test_generated_context_menus_reuse_shared_commands();
    test_tag_controller_round_trip_and_database_load();
    test_stash_controller_load_and_drop();
    gct_remove_dir(s_repo);
    TEST_END();
}
