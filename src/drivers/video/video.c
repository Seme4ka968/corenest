#include "video.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window   *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture = NULL;
static unsigned      g_tex_w = 0, g_tex_h = 0;
static uint32_t     *g_pixels = NULL;
static int           g_scale = 3;
static int           g_fullscreen = 0;
static int           g_fast_forward = 0;
static char          g_base_title[256] = "CoreNest";

bool video_init(unsigned width, unsigned height) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "video: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow(g_base_title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)width * g_scale, (int)height * g_scale,
        SDL_WINDOW_RESIZABLE);

    if (!g_window) {
        fprintf(stderr, "video: CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    {
        SDL_Surface *icon = SDL_LoadBMP("corenest.bmp");
        if (!icon) icon = SDL_LoadBMP("../corenest.bmp");
        if (icon) {
            SDL_SetWindowIcon(g_window, icon);
            SDL_FreeSurface(icon);
        }
    }

    g_renderer = SDL_CreateRenderer(g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        fprintf(stderr, "video: CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_RenderSetLogicalSize(g_renderer, (int)width, (int)height);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    g_tex_w = width;
    g_tex_h = height;
    g_pixels = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    if (!g_pixels) return false;

    g_texture = SDL_CreateTexture(g_renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        (int)width, (int)height);

    if (!g_texture) {
        fprintf(stderr, "video: CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void video_deinit(void) {
    if (g_texture)  SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window)   SDL_DestroyWindow(g_window);
    free(g_pixels);
    g_texture = NULL; g_renderer = NULL; g_window = NULL; g_pixels = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void video_toggle_fullscreen(void) {
    if (!g_window) return;
    g_fullscreen = !g_fullscreen;
    SDL_SetWindowFullscreen(g_window,
        g_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

int video_is_fullscreen(void) {
    return g_fullscreen;
}

void video_set_title(const char *title) {
    if (!g_window || !title) return;
    snprintf(g_base_title, sizeof(g_base_title), "%s", title);
    SDL_SetWindowTitle(g_window, g_base_title);
}

void video_set_scale(int scale) {
    if (!g_window || scale < 1) return;
    g_scale = scale;
    if (!g_fullscreen) {
        SDL_SetWindowSize(g_window,
            (int)g_tex_w * g_scale, (int)g_tex_h * g_scale);
    }
}

void video_set_position(int x, int y) {
    if (!g_window) return;
    SDL_SetWindowPosition(g_window, x, y);
}

int video_get_scale(void) {
    return g_scale;
}

void video_toggle_filter(void) {
    static int linear = 0;
    linear = !linear;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linear ? "1" : "0");
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = SDL_CreateTexture(g_renderer,
            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
            (int)g_tex_w, (int)g_tex_h);
    }
}

void video_set_fast_forward(int on) {
    g_fast_forward = on;
    if (g_renderer) {
        SDL_RenderSetVSync(g_renderer, on ? 0 : 1);
    }
}

int video_is_fast_forward(void) {
    return g_fast_forward;
}

int video_screenshot(const char *path) {
    if (!g_pixels || !g_tex_w || !g_tex_h || !path) return 0;

    SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(
        g_pixels,
        (int)g_tex_w, (int)g_tex_h,
        32, (int)(g_tex_w * 4),
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    if (!surf) return 0;

    int ok = SDL_SaveBMP(surf, path);
    SDL_FreeSurface(surf);

    return ok == 0;
}

void video_refresh(const void *data, unsigned width, unsigned height,
                   size_t pitch, enum retro_pixel_format fmt)
{
    if (!g_pixels || !data) return;

    if (width != g_tex_w || height != g_tex_h) {
        SDL_DestroyTexture(g_texture);
        g_tex_w = width; g_tex_h = height;
        free(g_pixels);
        g_pixels = (uint32_t*)malloc(width * height * sizeof(uint32_t));
        g_texture = SDL_CreateTexture(g_renderer,
            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
            (int)width, (int)height);
        SDL_RenderSetLogicalSize(g_renderer, (int)width, (int)height);
        if (!g_fullscreen) {
            SDL_SetWindowSize(g_window,
                (int)width * g_scale, (int)height * g_scale);
        }
    }

    const uint8_t *src = (const uint8_t*)data;

    for (unsigned y = 0; y < height; y++) {
        const uint8_t *row = src + y * pitch;
        uint32_t *dst = g_pixels + y * width;

        switch (fmt) {
        case RETRO_PIXEL_FORMAT_0RGB1555: {
            const uint16_t *p = (const uint16_t*)row;
            for (unsigned x = 0; x < width; x++) {
                uint16_t c = p[x];
                uint8_t r = (c >> 10) & 0x1F;
                uint8_t g = (c >>  5) & 0x1F;
                uint8_t b =  c        & 0x1F;
                r = (r << 3) | (r >> 2);
                g = (g << 3) | (g >> 2);
                b = (b << 3) | (b >> 2);
                dst[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
            }
        } break;

        case RETRO_PIXEL_FORMAT_RGB565: {
            const uint16_t *p = (const uint16_t*)row;
            for (unsigned x = 0; x < width; x++) {
                uint16_t c = p[x];
                uint8_t r = (c >> 11) & 0x1F;
                uint8_t g = (c >>  5) & 0x3F;
                uint8_t b =  c        & 0x1F;
                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);
                dst[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
            }
        } break;

        case RETRO_PIXEL_FORMAT_XRGB8888:
        default: {
            const uint32_t *p = (const uint32_t*)row;
            for (unsigned x = 0; x < width; x++)
                dst[x] = p[x] | (0xFFu << 24);
        } break;
        }
    }

    SDL_UpdateTexture(g_texture, NULL, g_pixels, (int)(width * sizeof(uint32_t)));
}

void video_present(void) {
    if (!g_renderer) return;
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}
