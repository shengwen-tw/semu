#pragma once

#if SEMU_HAS(VIRTIOGPU)
#include <SDL.h>

void window_init(void);
void window_add(uint32_t width, uint32_t height);
void window_render(uint32_t id,
                           uint32_t *image,
                           uint32_t bits_per_pixel,
                           uint32_t stride,
                           uint32_t sdl_format,
                           uint32_t width,
                           uint32_t height);
void display_resource_lock(uint32_t id);
void display_resource_unlock(uint32_t id);
#endif
