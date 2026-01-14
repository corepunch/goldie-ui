/**
 * Joystick Input Implementation (SDL Backend)
 * 
 * This module implements the joystick abstraction layer using SDL2.
 * All SDL-specific joystick code is isolated here, making it easier
 * to migrate to GLFW or other input libraries in the future.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#include "joystick.h"

// Internal state - SDL-specific implementation detail
static SDL_Joystick* g_joystick = NULL;

/**
 * Initialize joystick subsystem
 * 
 * Implementation Notes:
 * - Uses SDL_Init(SDL_INIT_JOYSTICK) to initialize joystick support
 * - Opens first available joystick device
 * - Enables joystick events in SDL event queue
 * 
 * GLFW Migration Path:
 * - Replace SDL_Init with GLFW initialization (already done for window/video)
 * - Replace SDL_JoystickOpen with glfwJoystickPresent/glfwGetJoystickName checks
 * - GLFW uses polling model (glfwGetGamepadState) vs SDL's event model,
 *   so event dispatch would need to poll in the main loop
 */
bool ui_joystick_init(void) {
  // Initialize SDL joystick subsystem if not already initialized
  if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0) {
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
      printf("SDL joystick subsystem could not initialize! SDL_Error: %s\n", 
             SDL_GetError());
      return false;
    }
  }
  
  // Open the first available joystick
  int num_joysticks = SDL_NumJoysticks();
  if (num_joysticks == 0) {
    printf("No joystick/gamepad devices found\n");
    return false;
  }
  
  for (int i = 0; i < num_joysticks; ++i) {
    g_joystick = SDL_JoystickOpen(i);
    if (g_joystick) {
      // Enable joystick events in SDL event queue
      SDL_JoystickEventState(SDL_ENABLE);
      printf("Opened joystick: %s\n", SDL_JoystickName(g_joystick));
      return true;
    } else {
      printf("Could not open joystick %d: %s\n", i, SDL_GetError());
    }
  }
  
  printf("Failed to open any joystick device\n");
  return false;
}

/**
 * Shutdown joystick subsystem
 * 
 * GLFW Migration Path:
 * - GLFW joysticks don't require explicit close
 * - This function would become a no-op or just clear internal state
 */
void ui_joystick_shutdown(void) {
  if (g_joystick) {
    SDL_JoystickClose(g_joystick);
    g_joystick = NULL;
  }
}

/**
 * Check if a joystick is currently connected
 */
bool ui_joystick_available(void) {
  return g_joystick != NULL;
}

/**
 * Get the name of the connected joystick
 * 
 * GLFW Migration Path:
 * - Replace SDL_JoystickName with glfwGetJoystickName(GLFW_JOYSTICK_1)
 */
const char* ui_joystick_get_name(void) {
  if (g_joystick) {
    return SDL_JoystickName(g_joystick);
  }
  return NULL;
}
