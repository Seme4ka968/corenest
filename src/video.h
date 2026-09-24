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

#endif
