// Notification/observer system for Form Editor.

#include "formeditor.h"
#include <stdlib.h>
#include <string.h>

#define MAX_SUBSCRIPTIONS 32

typedef struct {
  fe_observer_fn_t callback;
  void *ctx;
  bool active;
} subscription_t;

static subscription_t s_subscriptions[MAX_SUBSCRIPTIONS];
static int s_next_id = 0;

int fe_subscribe(fe_observer_fn_t callback, void *ctx) {
  if (!callback) return -1;
  
  // Find first available slot
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (!s_subscriptions[i].active) {
      s_subscriptions[i].callback = callback;
      s_subscriptions[i].ctx = ctx;
      s_subscriptions[i].active = true;
      return i;
    }
  }
  
  return -1; // No slots available
}

void fe_unsubscribe(int subscription_id) {
  if (subscription_id < 0 || subscription_id >= MAX_SUBSCRIPTIONS)
    return;
  
  s_subscriptions[subscription_id].active = false;
  s_subscriptions[subscription_id].callback = NULL;
  s_subscriptions[subscription_id].ctx = NULL;
}

void fe_notify(fe_event_type_t event, window_t *doc) {
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (s_subscriptions[i].active && s_subscriptions[i].callback) {
      s_subscriptions[i].callback(event, doc, s_subscriptions[i].ctx);
    }
  }
}
