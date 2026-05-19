# SocialFeed DBKit Migration Plan

Transform socialfeed from dual object-model to pure database-driven application with zero-boilerplate CRUD dialogs.

**Goal:** Achieve true NeXTSTEP DBKit-style declarative database UI with zero manual conversion code.

---

## Phase 1: Database Persistence (P1 Critical)

- [x] **1.1 Implement like count persistence** ✅ Commit 16efbd6
  - Add `app_like_post()` and `app_like_comment()` using `dbUpdate`
  - Replace in-memory `post_like()` calls in view_menubar.c and view_dlg_post.c
  - Verify likes persist across feed refresh

- [x] **1.2 Migrate comments to database** ✅ Commit 551d881
  - Update `app_add_comment()` to use `dbInsert(TABLE_COMMENTS)`
  - Replace `post_add_comment()` array manipulation
  - Remove manual `refresh_comments()` (tableview auto-refreshes)
  - Verify comments persist across dialog close/reopen

- [ ] **1.3 Add DB helper functions** *(optional)*
  - Extract `db_count_records()` helper
  - Extract `db_increment_field()` helper
  - Extract `db_iterate_results()` helper
  - Create controller_db_helpers.c for reusable patterns

---

## Phase 2: orionc Code Generation (P2 Important)

- [ ] **2.1 Generate ok_id/cancel_id from buttons**
  - Add `button_ids_t` tracking struct to orionc.c
  - Update `emit_controls_ex()` to parse `action="db.*.insert"` attributes
  - Detect cancel buttons by text/name
  - Emit actual button IDs instead of hardcoded zeros
  - Verify generated .h has non-zero ok_id/cancel_id

- [ ] **2.2 Generate db_fields metadata link**
  - Add `count_table_fields()` helper to orionc.c
  - Pass `database` xmlNodePtr through to `emit_form()`
  - Emit `.db_fields = posts_fields, .db_field_count = 6`
  - Verify generated .h has non-NULL db_fields pointer

---

## Phase 3: Combobox Foreign Key Binding (P2 Important)

- [ ] **3.1 Store item values in combobox**
  - Extend `combobox_state_t` with `int *values` array
  - Allocate values array in `evCreate`
  - Store value_field data during `cb_populate_from_database()`
  - Add `cbGetCurrentValue` message returning ID (not row index)
  - Free values array in `evDestroy`

- [ ] **3.2 Update binding generation**
  - Change orionc to emit `cbGetCurrentValue` for combobox bindings
  - Verify DDX extracts author ID (e.g., 1) not row index (e.g., 0)
  - Test in new post dialog: select "Alice" → author_id=1

---

## Phase 4: Dialog Migration (Recommended)

- [ ] **4.1 Replace manual new post dialog**
  - Remove `show_new_post_dialog()` implementation
  - Update view_menubar.c to use `show_db_dialog()` directly
  - Remove manual `app_add_post()` call (show_db_dialog inserts)
  - Verify File → New Post works with zero manual code

- [ ] **4.2 Replace manual new comment dialog**
  - Remove `show_new_comment_dialog()` implementation
  - Update view_dlg_post.c to use `show_db_dialog()` directly
  - Remove manual `app_add_comment()` call
  - Verify Add Comment works with zero manual code

---

## Phase 5: Object Model Elimination (Recommended)

- [ ] **5.1 Remove transient post_t/comment_t conversion**
  - Update post detail dialog to store `int post_id` instead of `post_t*`
  - Fetch `db_post_t*` on-demand via `dbFind`
  - Remove `app_get_post()` conversion helper
  - Update view layer to work directly with DB records

- [ ] **5.2 Delete object model entirely**
  - Remove model_feed.c file
  - Remove post_t and comment_t struct definitions
  - Remove all conversion helpers
  - Verify no memory leaks from allocated strings
  - Verify grep "post_t" in view layer returns zero

---

## Phase 6: XML Persistence (P2 Important)

- [ ] **6.1 Implement dbSave serialization**
  - Add `serialize_to_xml()` using libxml2
  - Serialize authors, posts, comments tables to XML
  - Update dbSave case to call serialization
  - Verify data persists across app restart
  - Check XML file contains all records

---

## Success Criteria

When complete, socialfeed should have:

- ✅ Zero manual object-to-DB conversion code
- ✅ Dialogs use show_db_dialog() with generated forms
- ✅ Mutations (likes, comments) persist automatically
- ✅ Foreign keys work via combobox value binding
- ✅ Master-detail filtering works declaratively
- ✅ Data survives application restart
- ✅ No memory leaks from string conversions
- ✅ View layer works directly with DB records

---

## Verification Commands

```bash
# Build and test after each phase
make clean && make
./build/bin/socialfeed

# Check for memory leaks (Phase 5)
leaks --atExit -- ./build/bin/socialfeed

# Verify generated forms (Phase 2)
grep -A5 "socialfeed_new_post_form" build/generated/examples/socialfeed/socialfeed.h

# Verify persistence (Phase 1, 6)
# Add data, kill app, restart, verify data still present
```

---

**Started:** 19 May 2026  
**Status:** Phase 1 in progress
