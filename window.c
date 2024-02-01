#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <SDL.h>

#include "virtio.h"
#include "window.h"

#define SDL_COND_TIMEOUT 1 /* ms */

struct display_info {
    uint32_t width;
    uint32_t height;
    uint32_t sdl_format;
    uint32_t *image;
    uint32_t bits_per_pixel;
    uint32_t stride;
    SDL_mutex *img_mtx;
    SDL_cond *img_cond;
    SDL_Thread *thread_id;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surface;
    SDL_Texture *texture;
};

static struct display_info displays[VIRTIO_GPU_MAX_SCANOUTS];
static int display_cnt;

void window_add(uint32_t width, uint32_t height)
{
    displays[display_cnt].width = width;
    displays[display_cnt].height = height;
    display_cnt++;
}

int window_thread(void *data)
{
    struct display_info *display = (struct display_info *) data;

    /* Create SDL window */
    display->window = SDL_CreateWindow("Semu", SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, display->width,
                                       display->height, SDL_WINDOW_SHOWN);

    if (!display->window) {
        fprintf(stderr, "%s(): failed to create window\n", __func__);
        exit(2);
    }

    /* Create SDL render */
    display->renderer =
        SDL_CreateRenderer(display->window, -1, SDL_RENDERER_ACCELERATED);

    if (!display->renderer) {
        fprintf(stderr, "%s(): failed to create renderer\n", __func__);
        exit(2);
    }

    while (1) {
        SDL_LockMutex(display->img_mtx);

        /* Wait until the image is arrived */
        while (SDL_CondWaitTimeout(display->img_cond, display->img_mtx,
                                   SDL_COND_TIMEOUT)) {
            /* Read event */
            SDL_Event e;
            SDL_PollEvent(&e);  // TODO: Handle events
        }

        /* Render image */
        display->surface = SDL_CreateRGBSurfaceWithFormatFrom(
            display->image, display->width, display->height,
            display->bits_per_pixel, display->stride, display->sdl_format);
        display->texture =
            SDL_CreateTextureFromSurface(display->renderer, display->surface);
        SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);
        SDL_RenderPresent(display->renderer);
        SDL_DestroyTexture(display->texture);

        SDL_UnlockMutex(display->img_mtx);
    }
}

void window_init(void)
{
    char thread_name[100] = {0};

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "%s(): failed to initialize SDL\n", __func__);
        exit(2);
    }

    for (int i = 0; i < display_cnt; i++) {
        displays[i].img_mtx = SDL_CreateMutex();
        displays[i].img_cond = SDL_CreateCond();

        sprintf(thread_name, "sdl thread %d", i);
        displays[i].thread_id = SDL_CreateThread(
            window_thread, thread_name, (void *) &displays[i]);
        SDL_DetachThread(displays[i].thread_id);
    }
}

void display_resource_lock(uint32_t id)
{
    SDL_LockMutex(displays[id].img_mtx);
}

void display_resource_unlock(uint32_t id)
{
    SDL_UnlockMutex(displays[id].img_mtx);
}

void window_render(uint32_t id,
                   uint32_t *image,
                   uint32_t bits_per_pixel,
                   uint32_t stride,
                   uint32_t sdl_format,
                   uint32_t width,
                   uint32_t height)
{
    displays[id].width = width;
    displays[id].height = height;
    displays[id].image = image;
    displays[id].bits_per_pixel = bits_per_pixel;
    displays[id].stride = stride;
    displays[id].sdl_format = sdl_format;
    SDL_CondSignal(displays[id].img_cond);
}
