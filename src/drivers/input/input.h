#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "libretro.h"

bool input_init(void);
void input_deinit(void);
void input_poll(void);
void input_refresh(void);
int16_t input_state(unsigned port, unsigned device,
                    unsigned index, unsigned id);

#endif
