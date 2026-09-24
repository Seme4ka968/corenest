#include "input.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static const uint8_t *g_keys = NULL;
static SDL_GameController *g_pad = NULL;

bool input_init(void) {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "input: SDL_Init GAMECONTROLLER failed: %s\n", SDL_GetError());
    }

    SDL_version v;
    SDL_GetVersion(&v);
    printf("[input] SDL %d.%d.%d\n", v.major, v.minor, v.patch);

    int n = SDL_NumJoysticks();
    printf("[input] joysticks: %d\n", n);

    for (int i = 0; i < n; i++) {
        const char *name = SDL_JoystickNameForIndex(i);
        int is_gc = SDL_IsGameController(i);
        printf("[input]   [%d] %s (gamecontroller: %s)\n",
               i, name ? name : "?", is_gc ? "yes" : "no");

        if (is_gc && !g_pad) {
            g_pad = SDL_GameControllerOpen(i);
            if (g_pad) {
                printf("[input] opened controller %d: %s\n",
                       i, SDL_GameControllerName(g_pad));
            }
        }
    }

    if (!g_pad) {
        printf("[input] no gamepad found, keyboard only\n");
    }
    return true;
}

void input_deinit(void) {
    if (g_pad) { SDL_GameControllerClose(g_pad); g_pad = NULL; }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
}

void input_poll(void) {
    SDL_PumpEvents();
    g_keys = SDL_GetKeyboardState(NULL);
}

static int16_t pad_button(SDL_GameControllerButton b) {
    if (!g_pad) return 0;
    return SDL_GameControllerGetButton(g_pad, b) ? 1 : 0;
}

static int16_t key_state(SDL_Scancode sc) {
    return (g_keys && g_keys[sc]) ? 1 : 0;
}

int16_t input_state(unsigned port, unsigned device,
                    unsigned index, unsigned id)
{
    (void)index;
    if (port != 0) return 0;

    if (device == RETRO_DEVICE_JOYPAD) {
        switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP:
            return pad_button(SDL_CONTROLLER_BUTTON_DPAD_UP) || key_state(SDL_SCANCODE_UP);
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            return pad_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || key_state(SDL_SCANCODE_DOWN);
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            return pad_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || key_state(SDL_SCANCODE_LEFT);
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            return pad_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || key_state(SDL_SCANCODE_RIGHT);
        case RETRO_DEVICE_ID_JOYPAD_A:
            return pad_button(SDL_CONTROLLER_BUTTON_B) || key_state(SDL_SCANCODE_X);
        case RETRO_DEVICE_ID_JOYPAD_B:
            return pad_button(SDL_CONTROLLER_BUTTON_A) || key_state(SDL_SCANCODE_Z);
        case RETRO_DEVICE_ID_JOYPAD_X:
            return pad_button(SDL_CONTROLLER_BUTTON_Y) || key_state(SDL_SCANCODE_S);
        case RETRO_DEVICE_ID_JOYPAD_Y:
            return pad_button(SDL_CONTROLLER_BUTTON_X) || key_state(SDL_SCANCODE_A);
        case RETRO_DEVICE_ID_JOYPAD_L:
            return pad_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || key_state(SDL_SCANCODE_Q);
        case RETRO_DEVICE_ID_JOYPAD_R:
            return pad_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || key_state(SDL_SCANCODE_W);
        case RETRO_DEVICE_ID_JOYPAD_START:
            return pad_button(SDL_CONTROLLER_BUTTON_START) || key_state(SDL_SCANCODE_RETURN);
        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            return pad_button(SDL_CONTROLLER_BUTTON_BACK) || key_state(SDL_SCANCODE_RSHIFT);
        default: return 0;
        }
    }
    return 0;
}
