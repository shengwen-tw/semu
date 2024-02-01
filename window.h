#pragma once

#ifdef ENABLE_SDL
#include <SDL.h>

void window_init(void);
void window_add(uint32_t width, uint32_t height);
void window_render(void *resource);
void display_resource_lock(uint32_t id);
void display_resource_unlock(uint32_t id);
#endif
