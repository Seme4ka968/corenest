#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool audio_init(double sample_rate);
void audio_deinit(void);
void audio_push_sample(int16_t left, int16_t right);
void audio_set_mute(int mute);
size_t audio_push_batch(const int16_t *data, size_t frames);

#endif
