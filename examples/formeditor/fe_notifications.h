// Notification/observer system for Form Editor.
// Allows panels to subscribe to document lifecycle and state-change events
// without tight coupling to specific refresh functions.

#ifndef __FE_NOTIFICATIONS_H__
#define __FE_NOTIFICATIONS_H__

#include "fe_document.h"

// Event types
typedef enum {
  FE_EVENT_DOCUMENT_CREATED,      // A new document was created
  FE_EVENT_DOCUMENT_CLOSED,       // A document was closed
  FE_EVENT_DOCUMENT_ACTIVATED,    // Active document changed
  FE_EVENT_DOCUMENT_MODIFIED,     // Document content was modified
  FE_EVENT_SELECTION_CHANGED,     // Canvas selection changed
  FE_EVENT_ELEMENT_ADDED,         // Element added to document
  FE_EVENT_ELEMENT_DELETED,       // Element deleted from document
  FE_EVENT_ELEMENT_MODIFIED,      // Element properties changed
  FE_EVENT_PROJECT_MODIFIED,      // Project-level change (plugins, etc.)
  FE_EVENT_COMPONENT_REGISTRY_CHANGED, // Component plugin loaded/unloaded
} fe_event_type_t;

// Observer callback signature
// ctx is the userdata registered with the subscription
typedef void (*fe_observer_fn_t)(fe_event_type_t event, form_doc_t *doc, void *ctx);

// Subscribe to events
// Returns subscription ID (>= 0) on success, -1 on failure
int fe_subscribe(fe_observer_fn_t callback, void *ctx);

// Unsubscribe from events
void fe_unsubscribe(int subscription_id);

// Broadcast an event to all subscribers
void fe_notify(fe_event_type_t event, form_doc_t *doc);

#endif // __FE_NOTIFICATIONS_H__
