#ifndef VIDEO_H
#define VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include "libretro.h"

bool video_init(unsigned width, unsigned height);
void video_deinit(void);
void video_refresh(const void *data, unsigned width, unsigned height,
                   size_t pitch, enum retro_pixel_format fmt);
void video_present(void);

void video_toggle_fullscreen(void);
void video_set_position(int x, int y);
void video_set_title(const char *title);
void video_set_scale(int scale);
int  video_get_scale(void);
void video_toggle_filter(void);
int  video_is_fullscreen(void);

#endif
