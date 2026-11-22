#include "psp/ctrl.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdio>

static int g_SamplingMode = PSP_CTRL_MODE_ANALOG;
static int g_SamplingCycle = 0;
static SceCtrlData g_CurrentPadState = { 0, 0, 128, 128, {0} };
static SDL_GameController* g_GameController = nullptr;

extern "C" {

int sceCtrlSetSamplingCycle(int cycle) {
    g_SamplingCycle = cycle;
    return 0;
}

int sceCtrlSetSamplingMode(int mode) {
    g_SamplingMode = mode;
    return 0;
}

int sceCtrlReadBufferPositive(SceCtrlData* pad_data, int count) {
    if (!pad_data || count <= 0) return 0;
    
    // Copy current mapped state
    for (int i = 0; i < count; ++i) {
        pad_data[i] = g_CurrentPadState;
        pad_data[i].TimeStamp = SDL_GetTicks();
    }
    return count;
}

void Input_PollController() {
    HAL_UpdateInputState();
}

} // extern "C"

void HAL_UpdateInputState() {
    u32 buttons = 0;
    u8 lx = 128;
    u8 ly = 128;

    // Open first available controller if not open
    if (!g_GameController && SDL_NumJoysticks() > 0) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) {
                g_GameController = SDL_GameControllerOpen(i);
                if (g_GameController) {
                    printf("[Input] Game Controller connected: %s\n", SDL_GameControllerName(g_GameController));
                    break;
                }
            }
        }
    }

    // 1. Keyboard fallback / mapping
    const Uint8* keystate = SDL_GetKeyboardState(nullptr);
    if (keystate) {
        // D-Pad
        if (keystate[SDL_SCANCODE_UP])     buttons |= PSP_CTRL_UP;
        if (keystate[SDL_SCANCODE_DOWN])   buttons |= PSP_CTRL_DOWN;
        if (keystate[SDL_SCANCODE_LEFT])   buttons |= PSP_CTRL_LEFT;
        if (keystate[SDL_SCANCODE_RIGHT])  buttons |= PSP_CTRL_RIGHT;

        // Action Buttons (PlayStation face layout: J = Square, K = Cross, I = Triangle, L = Circle)
        if (keystate[SDL_SCANCODE_K] || keystate[SDL_SCANCODE_SPACE])  buttons |= PSP_CTRL_CROSS;     // Jump / Accept
        if (keystate[SDL_SCANCODE_J])                                  buttons |= PSP_CTRL_SQUARE;    // Light Attack
        if (keystate[SDL_SCANCODE_I])                                  buttons |= PSP_CTRL_TRIANGLE;  // Heavy Attack
        if (keystate[SDL_SCANCODE_L] || keystate[SDL_SCANCODE_ESCAPE]) buttons |= PSP_CTRL_CIRCLE;    // Action / Cancel

        // Triggers / Bumpers (Q = L, E = R)
        if (keystate[SDL_SCANCODE_Q])     buttons |= PSP_CTRL_LTRIGGER;
        if (keystate[SDL_SCANCODE_E])     buttons |= PSP_CTRL_RTRIGGER;

        // Start / Select
        if (keystate[SDL_SCANCODE_RETURN]) buttons |= PSP_CTRL_START;
        if (keystate[SDL_SCANCODE_TAB])    buttons |= PSP_CTRL_SELECT;

        // Analog stick via WASD
        int stick_x = 0;
        int stick_y = 0;
        if (keystate[SDL_SCANCODE_A]) stick_x -= 127;
        if (keystate[SDL_SCANCODE_D]) stick_x += 127;
        if (keystate[SDL_SCANCODE_W]) stick_y -= 127;
        if (keystate[SDL_SCANCODE_S]) stick_y += 127;
        lx = (u8)(128 + stick_x);
        ly = (u8)(128 + stick_y);
    }

    // 2. Physical Gamepad mapping (if connected)
    if (g_GameController && SDL_GameControllerGetAttached(g_GameController)) {
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_DPAD_UP))    buttons |= PSP_CTRL_UP;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  buttons |= PSP_CTRL_DOWN;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  buttons |= PSP_CTRL_LEFT;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= PSP_CTRL_RIGHT;

        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_A))          buttons |= PSP_CTRL_CROSS;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_B))          buttons |= PSP_CTRL_CIRCLE;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_X))          buttons |= PSP_CTRL_SQUARE;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_Y))          buttons |= PSP_CTRL_TRIANGLE;

        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  buttons |= PSP_CTRL_LTRIGGER;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) buttons |= PSP_CTRL_RTRIGGER;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_START))         buttons |= PSP_CTRL_START;
        if (SDL_GameControllerGetButton(g_GameController, SDL_CONTROLLER_BUTTON_BACK))          buttons |= PSP_CTRL_SELECT;

        // Analog Left Stick
        Sint16 axis_x = SDL_GameControllerGetAxis(g_GameController, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 axis_y = SDL_GameControllerGetAxis(g_GameController, SDL_CONTROLLER_AXIS_LEFTY);
        // Deadzone threshold (8000 out of 32767)
        if (abs(axis_x) > 8000) {
            lx = (u8)((axis_x / 256) + 128);
        }
        if (abs(axis_y) > 8000) {
            ly = (u8)((axis_y / 256) + 128);
        }
    }

    g_CurrentPadState.Buttons = buttons;
    g_CurrentPadState.Lx = lx;
    g_CurrentPadState.Ly = ly;
}
