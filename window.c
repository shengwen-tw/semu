#include <pixman.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <SDL.h>

#include "virtio.h"
#include "window.h"

#define SDL_COND_TIMEOUT 1 /* ms */

int display_window_thread(void *data);

struct display_info {
    uint32_t width;
    uint32_t height;
    uint32_t sdl_format;
    pixman_image_t *image;
    SDL_mutex *img_mtx;
    SDL_cond *img_cond;
    SDL_Thread *thread_id;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surface;
    SDL_Texture *texture;
};

static struct display_info displays[VIRTIO_GPU_MAX_SCANOUTS];
int display_cnt;

void display_window_add(uint32_t width, uint32_t height)
{
    displays[display_cnt].width = width;
    displays[display_cnt].height = height;
    display_cnt++;
}

void display_window_init(void)
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
            display_window_thread, thread_name, (void *) &displays[i]);
        SDL_DetachThread(displays[i].thread_id);
    }
}

int display_window_thread(void *data)
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
        fprintf(stderr, "%s(): failed to createrenderer\n", __func__);
        exit(2);
    }

    while (1) {
        SDL_LockMutex(display->img_mtx);

        /* Wait until the image is arrived */
        while (SDL_CondWaitTimeout(display->img_cond, display->img_mtx,
                                   SDL_COND_TIMEOUT)) {
            /* Read event */
            SDL_Event e;
            if (SDL_PollEvent(&e)) {
                SDL_WaitEvent(&e);
            }
        }

        /* Prepare image */
        void *img_data = pixman_image_get_data(display->image);
        pixman_format_code_t format = pixman_image_get_format(display->image);
        uint32_t stride = pixman_image_get_stride(display->image);
        uint32_t depth = PIXMAN_FORMAT_BPP(format);

        /* Render image */
        display->surface = SDL_CreateRGBSurfaceWithFormatFrom(
            img_data, display->width, display->height, depth, stride,
            display->sdl_format);
        display->texture =
            SDL_CreateTextureFromSurface(display->renderer, display->surface);
        SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);
        SDL_RenderPresent(display->renderer);
        SDL_DestroyTexture(display->texture);

        SDL_UnlockMutex(display->img_mtx);
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

void display_window_render(uint32_t id,
                           pixman_image_t *image,
                           uint32_t sdl_format,
                           uint32_t width,
                           uint32_t height)
{
    displays[id].width = width;
    displays[id].height = height;
    displays[id].image = image;
    displays[id].sdl_format = sdl_format;
    SDL_CondSignal(displays[id].img_cond);
}
