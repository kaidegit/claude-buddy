#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#define SDL_MAIN_HANDLED

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <SDL.h>

#include "buddy_ui.h"
#include "buddy_ui_mock.h"
#include "lvgl.h"
#include "rtthread.h"
#include "sdl/sdl.h"
#include "sdl/sdl_common.h"

static void buddy_sim_hal_init(void);
static int buddy_sim_event_watch(void *userdata, SDL_Event *event);

int main(int argc, char **argv)
{
    uint32_t last_tick;

    (void)argc;
    (void)argv;

    lv_init();
    buddy_sim_hal_init();

    SDL_AddEventWatch(buddy_sim_event_watch, NULL);

    if (buddy_ui_init() != RT_EOK)
    {
        fprintf(stderr, "Buddy UI init failed\n");
        return 1;
    }

    rt_kprintf("Claude Buddy LVGL PC simulator\n");
    rt_kprintf("Keys: 1/Enter=primary, 2/Space=secondary, M/Tab=menu, N=next mock scene, B=previous mock scene, Q/Esc=quit\n");
    rt_kprintf("Buddy simulator scene: %s\n", buddy_ui_mock_scene_name());

    last_tick = rt_tick_get_millisecond();
    while (!sdl_quit_qry)
    {
        uint32_t now = rt_tick_get_millisecond();
        uint32_t elapsed = now - last_tick;
        uint32_t delay_ms;

        if (elapsed > 0)
        {
            lv_tick_inc(elapsed);
            last_tick = now;
        }

        buddy_ui_mock_tick(now);
        delay_ms = buddy_ui_run_once();
        if (delay_ms == 0)
        {
            delay_ms = 5;
        }
        SDL_Delay(delay_ms);
    }

    SDL_DelEventWatch(buddy_sim_event_watch, NULL);
    SDL_Quit();
    return 0;
}

static void buddy_sim_hal_init(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[SDL_HOR_RES * 80];
    static lv_color_t buf2[SDL_HOR_RES * 80];
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t mouse_drv;

    sdl_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SDL_HOR_RES * 80);

    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;
    disp_drv.antialiasing = 1;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&mouse_drv);
    mouse_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&mouse_drv);
}

static int buddy_sim_event_watch(void *userdata, SDL_Event *event)
{
    (void)userdata;

    if (event == NULL || event->type != SDL_KEYDOWN || event->key.repeat != 0)
    {
        return 0;
    }

    switch (event->key.keysym.sym)
    {
    case SDLK_1:
    case SDLK_RETURN:
    case SDLK_RIGHT:
        buddy_ui_sim_post_primary();
        break;
    case SDLK_2:
    case SDLK_SPACE:
    case SDLK_LEFT:
        buddy_ui_sim_post_secondary();
        break;
    case SDLK_m:
    case SDLK_TAB:
        buddy_ui_sim_post_menu();
        break;
    case SDLK_n:
        buddy_ui_mock_next_scene();
        break;
    case SDLK_b:
        buddy_ui_mock_previous_scene();
        break;
    case SDLK_q:
    case SDLK_ESCAPE:
        sdl_quit_qry = true;
        break;
    default:
        break;
    }

    return 0;
}
