// Test environment for UI window and message testing
// Provides utilities to create windows, send messages, and track events

#ifndef __TEST_ENV_H__
#define __TEST_ENV_H__

#include "../ui.h"
#include <stdbool.h>
#include <stdio.h>

// Test event tracking structure
typedef struct {
    window_t *window;
    uint32_t msg;
    uint32_t wparam;
    void *lparam;
    int call_count;
} test_event_t;

// Test environment state
#define MAX_TRACKED_EVENTS 100

typedef struct {
    test_event_t events[MAX_TRACKED_EVENTS];
    int event_count;
    bool tracking_enabled;
} test_env_t;

// Global test environment
extern test_env_t test_env;

// Initialize test environment
void test_env_init(void);

// Shutdown test environment
void test_env_shutdown(void);

// Enable/disable event tracking
void test_env_enable_tracking(bool enable);

// Clear tracked events
void test_env_clear_events(void);

// Get number of tracked events
int test_env_get_event_count(void);

// Get specific tracked event
test_event_t* test_env_get_event(int index);

// Find event by message type
test_event_t* test_env_find_event(uint32_t msg);

// Check if a message was sent
bool test_env_was_message_sent(uint32_t msg);

// Count how many times a message was sent
int test_env_count_message(uint32_t msg);

// Hook callback for tracking events
void test_env_hook_callback(window_t *win, uint32_t msg, uint32_t wparam, 
                            void *lparam, void *userdata);

// Helper: Create a test window with tracking
window_t* test_env_create_window(const char *title, int x, int y, int w, int h,
                                  winproc_t proc, void *userdata);

// Helper: Send a tracked message
int test_env_send_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Helper: Post a tracked message
void test_env_post_message(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

#endif // __TEST_ENV_H__
