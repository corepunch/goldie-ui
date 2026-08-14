#ifndef __GC_ACTIONS_H__
#define __GC_ACTIONS_H__

#include "gitclient.h"

// Outcome of the canonical action dispatch boundary. The implementation
// migration can add finer-grained backend results without changing callers.
typedef enum {
  GC_ACTION_DONE,          // a known action handler was invoked
  GC_ACTION_UNAVAILABLE,   // no repo / no selection / unsupported state
  GC_ACTION_CANCELLED,     // user declined a confirmation prompt
  GC_ACTION_FAILED,        // backend reported failure
} gc_action_result_t;

// The canonical execution path: trace, dispatch to the single handler for the
// action ID, trace the result. Menu, toolbar, context menu, accelerator, and
// test invocations all converge here.
gc_action_result_t gc_execute_action(uint16_t id);

// Returns true if an action handler exists for `id`, and fills `name` with the
// action's symbolic name. Used by tests to assert every declared action (and
// every toolbar button) has a handler.
bool gc_action_handler_for(uint16_t id, const char **name);

#endif // __GC_ACTIONS_H__
