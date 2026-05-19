// VIEW: Form-based dialogs — REMOVED.
//
// This file previously contained manual dialog implementations for New Post
// and New Comment. Both have been migrated to show_db_dialog() / show_db_dialog_ex().
//
// See:
//  - New Post:    view_menubar.c ID_POST_NEW handler
//  - New Comment: view_dlg_post.c ID_POST_DETAIL_ADD_COMMENT handler
//
// All form layouts are generated from socialfeed.orion.
// All database operations are handled automatically by the framework.
// No manual dialog procs, bindings, or validation code needed.

#include "socialfeed.h"

// File intentionally left mostly empty — kept for build system compatibility.
// May be removed entirely in future cleanup.
