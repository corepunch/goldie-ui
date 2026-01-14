#ifndef __UI_JOYSTICK_H__
#define __UI_JOYSTICK_H__

/**
 * Joystick Input Abstraction Layer
 * 
 * This module provides an abstraction layer for joystick/gamepad input,
 * decoupling the UI framework from SDL-specific joystick APIs.
 * 
 * Design Goals:
 * 1. Centralize all joystick initialization and management in the UI layer
 * 2. Provide a clean abstraction that can be implemented with different
 *    backends (SDL, GLFW, etc.) without changing application code
 * 3. Route joystick events through the existing window message system
 * 
 * SDL to GLFW Migration Notes:
 * - This API abstracts SDL_Joystick functions to prepare for potential
 *   migration to GLFW's gamepad API (glfwGetGamepadState, etc.)
 * - The opaque joystick handle allows backend implementation changes
 * - All SDL-specific types are kept internal to the implementation
 */

#include <stdbool.h>

// Opaque joystick handle (hides SDL_Joystick* implementation detail)
typedef void* ui_joystick_t;

/**
 * Initialize joystick subsystem
 * Opens the first available joystick/gamepad device.
 * 
 * Returns:
 *   true if a joystick was successfully opened, false otherwise
 * 
 * Note: This function internally calls SDL_Init(SDL_INIT_JOYSTICK) and
 *       SDL_JoystickOpen(). When migrating to GLFW, this would be replaced
 *       with GLFW joystick initialization.
 */
bool ui_joystick_init(void);

/**
 * Shutdown joystick subsystem
 * Closes any open joystick devices.
 * 
 * Note: When migrating to GLFW, this cleanup logic would be adapted
 *       for GLFW's joystick lifecycle.
 */
void ui_joystick_shutdown(void);

/**
 * Check if a joystick is currently connected
 * 
 * Returns:
 *   true if a joystick is available, false otherwise
 */
bool ui_joystick_available(void);

/**
 * Get the name of the connected joystick
 * 
 * Returns:
 *   Joystick name string, or NULL if no joystick is connected
 */
const char* ui_joystick_get_name(void);

#endif
