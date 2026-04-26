#include "buddy_ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ascii/buddy_ascii.h"
#include "buddy_ui_data.h"
#include "littlevgl2rtt.h"
#include "lv_ex_data.h"
#include "lvgl.h"
#include "lv_gif.h"
#include "lvsf_font.h"
#include "rtthread.h"

#ifdef USING_BUTTON_LIB
#include "board.h"
#include "button.h"
#endif

#define BUDDY_UI_REFRESH_MS 250U
#define BUDDY_UI_MAX_HANDLER_DELAY_MS 20U
#define BUDDY_UI_MAX_WAIT_WARNING_S 30U
#define BUDDY_UI_SETTINGS_COUNT 6U
#define BUDDY_UI_DEFAULT_BRIGHTNESS 80U

#if LV_FONT_MONTSERRAT_36
#define BUDDY_UI_BUILTIN_FONT_HERO (&lv_font_montserrat_36)
#else
#define BUDDY_UI_BUILTIN_FONT_HERO LV_FONT_DEFAULT
#endif

#if LV_FONT_MONTSERRAT_24
#define BUDDY_UI_BUILTIN_FONT_TITLE (&lv_font_montserrat_24)
#else
#define BUDDY_UI_BUILTIN_FONT_TITLE LV_FONT_DEFAULT
#endif

#if LV_FONT_MONTSERRAT_16
#define BUDDY_UI_BUILTIN_FONT_BODY (&lv_font_montserrat_16)
#elif LV_FONT_MONTSERRAT_20
#define BUDDY_UI_BUILTIN_FONT_BODY (&lv_font_montserrat_20)
#else
#define BUDDY_UI_BUILTIN_FONT_BODY LV_FONT_DEFAULT
#endif

#if LV_FONT_MONTSERRAT_12
#define BUDDY_UI_BUILTIN_FONT_SMALL (&lv_font_montserrat_12)
#else
#define BUDDY_UI_BUILTIN_FONT_SMALL LV_FONT_DEFAULT
#endif

#define BUDDY_UI_FONT_HERO buddy_ui_font_hero()
#define BUDDY_UI_FONT_TITLE buddy_ui_font_title()
#define BUDDY_UI_FONT_BODY buddy_ui_font_body()
#define BUDDY_UI_FONT_SMALL buddy_ui_font_small()
#define BUDDY_UI_FONT_ASCII buddy_ui_font_ascii()

typedef enum
{
    BUDDY_UI_SCREEN_HOME = 0,
    BUDDY_UI_SCREEN_PET,
    BUDDY_UI_SCREEN_INFO,
    BUDDY_UI_SCREEN_SETTINGS,
} buddy_ui_screen_t;

typedef enum
{
    BUDDY_UI_VIEW_HOME = 0,
    BUDDY_UI_VIEW_PET_STATS,
    BUDDY_UI_VIEW_APPROVAL,
    BUDDY_UI_VIEW_PAIRING,
    BUDDY_UI_VIEW_INFO,
    BUDDY_UI_VIEW_SETTINGS,
} buddy_ui_view_t;

typedef enum
{
    BUDDY_UI_ACTION_PRIMARY = 1U << 0,
    BUDDY_UI_ACTION_SECONDARY = 1U << 1,
    BUDDY_UI_ACTION_MENU = 1U << 2,
    BUDDY_UI_ACTION_APPROVE = 1U << 3,
    BUDDY_UI_ACTION_DENY = 1U << 4,
} buddy_ui_action_t;

typedef enum
{
    BUDDY_UI_PROMPT_DECISION_NONE = 0,
    BUDDY_UI_PROMPT_DECISION_APPROVE_SENT,
    BUDDY_UI_PROMPT_DECISION_DENY_SENT,
    BUDDY_UI_PROMPT_DECISION_SEND_FAILED,
} buddy_ui_prompt_decision_t;

typedef struct
{
    lv_coord_t space_sm;
    lv_coord_t space_md;
    lv_coord_t space_lg;
    lv_coord_t radius;
} buddy_ui_metrics_t;

static lv_obj_t *s_header_row;
static lv_obj_t *s_title_label;
static lv_obj_t *s_page_label;
static lv_obj_t *s_nav_label;

static lv_obj_t *s_home_page;
static lv_obj_t *s_home_status_row;
static lv_obj_t *s_home_character_frame;
static lv_obj_t *s_home_character_gif;
static lv_obj_t *s_home_persona_label;
static lv_obj_t *s_home_summary_label;
static lv_obj_t *s_home_entries_label;

static lv_obj_t *s_pet_page;
static lv_obj_t *s_pet_stats_label;

static lv_obj_t *s_approval_page;
static lv_obj_t *s_approval_tool_label;
static lv_obj_t *s_approval_hint_label;
static lv_obj_t *s_approval_wait_label;
static lv_obj_t *s_approval_status_label;

static lv_obj_t *s_pairing_page;
static lv_obj_t *s_pairing_passkey_label;
static lv_obj_t *s_pairing_hint_label;

static lv_obj_t *s_info_page;
static lv_obj_t *s_info_label;

static lv_obj_t *s_settings_page;
static lv_obj_t *s_settings_brightness_label;
static lv_obj_t *s_settings_brightness_bar;
static lv_obj_t *s_settings_sound_switch;
static lv_obj_t *s_settings_led_switch;
static lv_obj_t *s_settings_transcript_switch;
static lv_obj_t *s_settings_pet_label;
static lv_obj_t *s_settings_reset_label;
static lv_obj_t *s_settings_focus_label;

static uint32_t s_last_refresh_ms;
static volatile uint32_t s_pending_actions;
static bool s_ready;
static buddy_ui_screen_t s_screen = BUDDY_UI_SCREEN_HOME;
static buddy_ui_view_t s_view = BUDDY_UI_VIEW_HOME;
static buddy_ui_prompt_decision_t s_prompt_decision;
static char s_prompt_id[BUDDY_UI_PROMPT_ID_LEN];
static char s_home_character_path[BUDDY_CHARACTER_RUNTIME_PATH_LEN];

static uint8_t s_brightness = BUDDY_UI_DEFAULT_BRIGHTNESS;
static bool s_sound_enabled = true;
static bool s_led_enabled = true;
static bool s_transcript_enabled = true;
static bool s_reset_armed;
static uint8_t s_settings_index;
static uint8_t s_info_page_index;
static uint8_t s_transcript_page_index;

#ifdef USING_BUTTON_LIB
static int32_t s_key1_id = -1;
static int32_t s_key2_id = -1;
#endif

static void buddy_ui_set_hidden(lv_obj_t *obj, bool hidden);

static lv_coord_t buddy_ui_short_side(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t hor = lv_disp_get_hor_res(disp);
    lv_coord_t ver = lv_disp_get_ver_res(disp);

    return hor < ver ? hor : ver;
}

#ifdef LV_USING_FREETYPE_ENGINE
static const lv_font_t *buddy_ui_cjk_font(uint16_t size, const lv_font_t *fallback)
{
    lv_font_t *font = lvsf_get_font_by_name("DroidSansFallback", size);

    return font != NULL ? font : fallback;
}
#endif

static const lv_font_t *buddy_ui_font_hero(void)
{
#ifdef LV_USING_FREETYPE_ENGINE
    return buddy_ui_cjk_font(FONT_BIGL, BUDDY_UI_BUILTIN_FONT_HERO);
#else
    return BUDDY_UI_BUILTIN_FONT_HERO;
#endif
}

static const lv_font_t *buddy_ui_font_title(void)
{
#ifdef LV_USING_FREETYPE_ENGINE
    return buddy_ui_cjk_font(FONT_TITLE, BUDDY_UI_BUILTIN_FONT_TITLE);
#else
    return BUDDY_UI_BUILTIN_FONT_TITLE;
#endif
}

static const lv_font_t *buddy_ui_font_body(void)
{
#ifdef LV_USING_FREETYPE_ENGINE
    return buddy_ui_cjk_font(FONT_NORMAL, BUDDY_UI_BUILTIN_FONT_BODY);
#else
    return BUDDY_UI_BUILTIN_FONT_BODY;
#endif
}

static const lv_font_t *buddy_ui_font_small(void)
{
#ifdef LV_USING_FREETYPE_ENGINE
    return buddy_ui_cjk_font(FONT_SMALL, BUDDY_UI_BUILTIN_FONT_SMALL);
#else
    return BUDDY_UI_BUILTIN_FONT_SMALL;
#endif
}

static const lv_font_t *buddy_ui_font_ascii(void)
{
#if LV_FONT_UNSCII_8 && LV_FONT_UNSCII_16
    return buddy_ui_short_side() <= 390 ? &lv_font_unscii_8 : &lv_font_unscii_16;
#elif LV_FONT_UNSCII_8
    return &lv_font_unscii_8;
#elif LV_FONT_UNSCII_16
    return &lv_font_unscii_16;
#else
    return BUDDY_UI_BUILTIN_FONT_SMALL;
#endif
}

static lv_coord_t buddy_ui_ascii_label_height(const lv_font_t *font)
{
    lv_coord_t line_height = font != NULL ? (lv_coord_t)lv_font_get_line_height(font) : 0;
    const lv_coord_t short_side = buddy_ui_short_side();

    if (line_height <= 0)
    {
        line_height = short_side <= 390 ? 8 : 16;
    }

    return (lv_coord_t)((line_height * BUDDY_ASCII_ROWS) + (short_side <= 390 ? 4 : 8));
}

static void buddy_ui_reset_settings_to_defaults(void)
{
    s_brightness = BUDDY_UI_DEFAULT_BRIGHTNESS;
    s_sound_enabled = true;
    s_led_enabled = true;
    s_transcript_enabled = true;
}

static void buddy_ui_load_settings(void)
{
    buddy_ui_settings_t settings;

    if (!buddy_ui_data_load_settings(&settings))
    {
        return;
    }

    s_brightness = settings.brightness;
    s_sound_enabled = settings.sound_enabled;
    s_led_enabled = settings.led_enabled;
    s_transcript_enabled = settings.transcript_enabled;
}

static void buddy_ui_save_settings(void)
{
    buddy_ui_settings_t settings;

    settings.brightness = s_brightness;
    settings.sound_enabled = s_sound_enabled;
    settings.led_enabled = s_led_enabled;
    settings.transcript_enabled = s_transcript_enabled;
    buddy_ui_data_save_settings(&settings);
}

static void buddy_ui_release_character_gif(void)
{
#if LV_USE_GIF
    if (s_home_character_gif != NULL)
    {
        lv_obj_del(s_home_character_gif);
        s_home_character_gif = NULL;
    }
    if (s_home_character_frame != NULL)
    {
        s_home_character_gif = lv_gif_create(s_home_character_frame);
        lv_obj_center(s_home_character_gif);
        buddy_ui_set_hidden(s_home_character_frame, true);
    }
#endif
    s_home_character_path[0] = '\0';
}

static bool buddy_ui_run_factory_reset(void)
{
    buddy_ui_release_character_gif();
    if (!buddy_ui_data_factory_reset())
    {
        return false;
    }

    buddy_ui_reset_settings_to_defaults();
    s_reset_armed = false;
    s_settings_index = 0;
    s_info_page_index = 0;
    s_transcript_page_index = 0;
    s_prompt_id[0] = '\0';
    s_prompt_decision = BUDDY_UI_PROMPT_DECISION_NONE;
    s_screen = BUDDY_UI_SCREEN_HOME;
    return true;
}

static buddy_ui_metrics_t buddy_ui_metrics(void)
{
    buddy_ui_metrics_t metrics;
    lv_coord_t short_side = buddy_ui_short_side();

    if (short_side <= 240)
    {
        metrics.space_sm = 3;
        metrics.space_md = 5;
        metrics.space_lg = 8;
        metrics.radius = 4;
    }
    else if (short_side <= 390)
    {
        metrics.space_sm = 4;
        metrics.space_md = 7;
        metrics.space_lg = 12;
        metrics.radius = 6;
    }
    else
    {
        metrics.space_sm = 8;
        metrics.space_md = 14;
        metrics.space_lg = 20;
        metrics.radius = 8;
    }

    return metrics;
}

static lv_coord_t buddy_ui_character_frame_height(void)
{
    lv_coord_t short_side = buddy_ui_short_side();

    if (short_side <= 240)
    {
        return 66;
    }
    if (short_side <= 390)
    {
        return 68;
    }
    return 120;
}

static void buddy_ui_post_action(buddy_ui_action_t action)
{
    rt_base_t level = rt_hw_interrupt_disable();
    s_pending_actions |= (uint32_t)action;
    rt_hw_interrupt_enable(level);
}

static uint32_t buddy_ui_take_actions(void)
{
    uint32_t actions;
    rt_base_t level = rt_hw_interrupt_disable();

    actions = s_pending_actions;
    s_pending_actions = 0;
    rt_hw_interrupt_enable(level);
    return actions;
}

static lv_obj_t *buddy_ui_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);

    return label;
}

static lv_obj_t *buddy_ui_page(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);

    lv_obj_set_width(page, lv_pct(100));
    lv_obj_set_flex_grow(page, 1);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_pad_row(page, buddy_ui_metrics().space_md, 0);

    return page;
}

static void buddy_ui_button_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
        buddy_ui_post_action((buddy_ui_action_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

static lv_obj_t *buddy_ui_button(lv_obj_t *parent,
                                 const char *text,
                                 lv_color_t color,
                                 buddy_ui_action_t action)
{
    buddy_ui_metrics_t metrics = buddy_ui_metrics();
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label;

    lv_obj_set_height(button, 48);
    lv_obj_set_flex_grow(button, 1);
    lv_obj_set_style_radius(button, metrics.radius, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_add_event_cb(button, buddy_ui_button_event, LV_EVENT_CLICKED, (void *)(uintptr_t)action);

    label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, BUDDY_UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF3F6FA), 0);
    lv_obj_center(label);
    return button;
}

static void buddy_ui_set_hidden(lv_obj_t *obj, bool hidden)
{
    if (obj == NULL)
    {
        return;
    }

    if (hidden)
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static const char *buddy_ui_persona_text(buddy_ui_persona_t persona)
{
    switch (persona)
    {
    case BUDDY_UI_PERSONA_BUSY:
        return "BUSY";
    case BUDDY_UI_PERSONA_ATTENTION:
        return "ATTENTION";
    case BUDDY_UI_PERSONA_CELEBRATE:
        return "CELEBRATE";
    case BUDDY_UI_PERSONA_DIZZY:
        return "DIZZY";
    case BUDDY_UI_PERSONA_HEART:
        return "HEART";
    case BUDDY_UI_PERSONA_IDLE:
        return "IDLE";
    case BUDDY_UI_PERSONA_SLEEP:
    default:
        return "SLEEP";
    }
}

static lv_color_t buddy_ui_persona_color(buddy_ui_persona_t persona)
{
    switch (persona)
    {
    case BUDDY_UI_PERSONA_ATTENTION:
        return lv_color_hex(0xFFB454);
    case BUDDY_UI_PERSONA_BUSY:
        return lv_color_hex(0x72D6FF);
    case BUDDY_UI_PERSONA_IDLE:
        return lv_color_hex(0xA8E6A1);
    default:
        return lv_color_hex(0xB6BCC8);
    }
}

static lv_color_t buddy_ui_color_from_rgb565(uint16_t color)
{
    return lv_color_make((uint8_t)((((color >> 11) & 0x1FU) * 255U) / 31U),
                         (uint8_t)((((color >> 5) & 0x3FU) * 255U) / 63U),
                         (uint8_t)(((color & 0x1FU) * 255U) / 31U));
}

static buddy_character_state_t buddy_ui_character_state(buddy_ui_persona_t persona)
{
    switch (persona)
    {
    case BUDDY_UI_PERSONA_BUSY:
        return BUDDY_CHARACTER_STATE_BUSY;
    case BUDDY_UI_PERSONA_ATTENTION:
        return BUDDY_CHARACTER_STATE_ATTENTION;
    case BUDDY_UI_PERSONA_CELEBRATE:
        return BUDDY_CHARACTER_STATE_CELEBRATE;
    case BUDDY_UI_PERSONA_DIZZY:
        return BUDDY_CHARACTER_STATE_DIZZY;
    case BUDDY_UI_PERSONA_HEART:
        return BUDDY_CHARACTER_STATE_HEART;
    case BUDDY_UI_PERSONA_IDLE:
        return BUDDY_CHARACTER_STATE_IDLE;
    case BUDDY_UI_PERSONA_SLEEP:
    default:
        return BUDDY_CHARACTER_STATE_SLEEP;
    }
}

static bool buddy_ui_model_prefers_gif(const buddy_ui_model_t *model)
{
    return model->species == model->gif_species;
}

static uint8_t buddy_ui_effective_ascii_species(const buddy_ui_model_t *model)
{
    if (model->species < model->species_count)
    {
        return model->species;
    }

    return 0;
}

static uint8_t buddy_ui_next_species(const buddy_ui_model_t *model)
{
    const bool gif_available = buddy_ui_data_character_available();
    const uint8_t count = model->species_count > 0 ? model->species_count : buddy_ascii_species_count();
    const uint8_t current = buddy_ui_effective_ascii_species(model);

    if (buddy_ui_model_prefers_gif(model) && gif_available)
    {
        return 0;
    }

    if (current + 1U >= count)
    {
        return gif_available ? model->gif_species : 0;
    }

    return (uint8_t)(current + 1U);
}

static void buddy_ui_cycle_pet(const buddy_ui_model_t *model)
{
    (void)buddy_ui_data_set_species(buddy_ui_next_species(model));
    buddy_ui_data_character_invalidate();
}

static const char *buddy_ui_screen_name(buddy_ui_view_t view)
{
    switch (view)
    {
    case BUDDY_UI_VIEW_PET_STATS:
        return "Pet";
    case BUDDY_UI_VIEW_APPROVAL:
        return "Approval";
    case BUDDY_UI_VIEW_PAIRING:
        return "Pairing";
    case BUDDY_UI_VIEW_INFO:
        return "Info";
    case BUDDY_UI_VIEW_SETTINGS:
        return "Settings";
    case BUDDY_UI_VIEW_HOME:
    default:
        return "Home";
    }
}

static uint32_t buddy_ui_stack_used_high_water(void)
{
    rt_thread_t thread = rt_thread_self();
    rt_uint8_t *start;
    rt_uint8_t *end;
    rt_uint8_t *ptr;

    if (thread == RT_NULL || thread->stack_addr == RT_NULL || thread->stack_size == 0)
    {
        return 0;
    }

    start = (rt_uint8_t *)thread->stack_addr;
    end = start + thread->stack_size;

#ifdef ARCH_CPU_STACK_GROWS_UPWARD
    ptr = end;
    while (ptr > start && *(ptr - 1) == '#')
    {
        --ptr;
    }
    return (uint32_t)(ptr - start);
#else
    ptr = start;
    while (ptr < end && *ptr == '#')
    {
        ++ptr;
    }
    return (uint32_t)(end - ptr);
#endif
}

static void buddy_ui_log_home_approval_transition(buddy_ui_view_t old_view, buddy_ui_view_t new_view)
{
    rt_uint32_t heap_total = 0;
    rt_uint32_t heap_used = 0;
    rt_uint32_t heap_max_used = 0;
    rt_uint32_t heap_free_min = 0;
    uint32_t stack_used_hwm;
    rt_thread_t thread;

    if (!((old_view == BUDDY_UI_VIEW_HOME && new_view == BUDDY_UI_VIEW_APPROVAL) ||
          (old_view == BUDDY_UI_VIEW_APPROVAL && new_view == BUDDY_UI_VIEW_HOME)))
    {
        return;
    }

    rt_memory_info(&heap_total, &heap_used, &heap_max_used);
    if (heap_total > heap_max_used)
    {
        heap_free_min = heap_total - heap_max_used;
    }

    stack_used_hwm = buddy_ui_stack_used_high_water();
    thread = rt_thread_self();
    rt_kprintf("Buddy UI %s->%s heap_free_min=%lu stack_used_hwm=%lu stack_free_min=%lu stack_size=%lu\n",
               buddy_ui_screen_name(old_view),
               buddy_ui_screen_name(new_view),
               (unsigned long)heap_free_min,
               (unsigned long)stack_used_hwm,
               (unsigned long)(thread != RT_NULL && thread->stack_size > stack_used_hwm ?
                                   thread->stack_size - stack_used_hwm : 0),
               (unsigned long)(thread != RT_NULL ? thread->stack_size : 0));
    (void)heap_used;
}

static buddy_ui_view_t buddy_ui_effective_view(const buddy_ui_model_t *model)
{
    if (model->has_prompt)
    {
        return BUDDY_UI_VIEW_APPROVAL;
    }

    if (model->has_pairing_passkey)
    {
        return BUDDY_UI_VIEW_PAIRING;
    }

    switch (s_screen)
    {
    case BUDDY_UI_SCREEN_PET:
        return BUDDY_UI_VIEW_PET_STATS;
    case BUDDY_UI_SCREEN_INFO:
        return BUDDY_UI_VIEW_INFO;
    case BUDDY_UI_SCREEN_SETTINGS:
        return BUDDY_UI_VIEW_SETTINGS;
    case BUDDY_UI_SCREEN_HOME:
    default:
        return BUDDY_UI_VIEW_HOME;
    }
}

static void buddy_ui_next_screen(void)
{
    switch (s_screen)
    {
    case BUDDY_UI_SCREEN_HOME:
        s_screen = BUDDY_UI_SCREEN_PET;
        break;
    case BUDDY_UI_SCREEN_PET:
        s_screen = BUDDY_UI_SCREEN_INFO;
        break;
    case BUDDY_UI_SCREEN_INFO:
        s_screen = BUDDY_UI_SCREEN_SETTINGS;
        break;
    case BUDDY_UI_SCREEN_SETTINGS:
    default:
        s_screen = BUDDY_UI_SCREEN_HOME;
        break;
    }
}

static void buddy_ui_adjust_settings(const buddy_ui_model_t *model)
{
    const uint8_t index = s_settings_index;

    switch (index)
    {
    case 0:
        s_brightness = (uint8_t)(s_brightness >= 100 ? 20 : s_brightness + 20);
        break;
    case 1:
        s_sound_enabled = !s_sound_enabled;
        break;
    case 2:
        s_led_enabled = !s_led_enabled;
        break;
    case 3:
        s_transcript_enabled = !s_transcript_enabled;
        break;
    case 4:
        buddy_ui_cycle_pet(model);
        break;
    case 5:
    default:
        if (s_reset_armed)
        {
            buddy_ui_run_factory_reset();
        }
        else
        {
            s_reset_armed = true;
        }
        break;
    }

    if (index < 4U)
    {
        buddy_ui_save_settings();
    }
    s_settings_index = (uint8_t)((s_settings_index + 1U) % BUDDY_UI_SETTINGS_COUNT);
}

static bool buddy_ui_process_actions(const buddy_ui_model_t *model)
{
    const uint32_t actions = buddy_ui_take_actions();
    bool changed = false;

    if (actions == 0)
    {
        return false;
    }

    lv_disp_trig_activity(NULL);

    if ((actions & BUDDY_UI_ACTION_MENU) != 0)
    {
        s_screen = BUDDY_UI_SCREEN_SETTINGS;
        changed = true;
    }

    if (model->has_prompt)
    {
        if ((actions & (BUDDY_UI_ACTION_APPROVE | BUDDY_UI_ACTION_PRIMARY)) != 0)
        {
            if (buddy_ui_data_send_permission_once())
            {
                s_prompt_decision = BUDDY_UI_PROMPT_DECISION_APPROVE_SENT;
            }
            else if (s_prompt_decision == BUDDY_UI_PROMPT_DECISION_NONE)
            {
                s_prompt_decision = BUDDY_UI_PROMPT_DECISION_SEND_FAILED;
            }
            changed = true;
        }
        if ((actions & (BUDDY_UI_ACTION_DENY | BUDDY_UI_ACTION_SECONDARY)) != 0)
        {
            if (buddy_ui_data_send_permission_deny())
            {
                s_prompt_decision = BUDDY_UI_PROMPT_DECISION_DENY_SENT;
            }
            else if (s_prompt_decision == BUDDY_UI_PROMPT_DECISION_NONE)
            {
                s_prompt_decision = BUDDY_UI_PROMPT_DECISION_SEND_FAILED;
            }
            changed = true;
        }

        return changed;
    }

    if ((actions & BUDDY_UI_ACTION_PRIMARY) != 0)
    {
        buddy_ui_next_screen();
        changed = true;
    }

    if ((actions & BUDDY_UI_ACTION_SECONDARY) != 0)
    {
        if (s_screen == BUDDY_UI_SCREEN_INFO)
        {
            s_info_page_index = (uint8_t)((s_info_page_index + 1U) % 2U);
        }
        else if (s_screen == BUDDY_UI_SCREEN_PET)
        {
            s_transcript_page_index = 0;
        }
        else if (s_screen == BUDDY_UI_SCREEN_SETTINGS)
        {
            buddy_ui_adjust_settings(model);
        }
        else
        {
            s_transcript_page_index = (uint8_t)((s_transcript_page_index + 1U) % 2U);
        }
        changed = true;
    }

    return changed;
}

static void buddy_ui_join_entries(char *out, size_t out_size, const buddy_ui_model_t *model)
{
    size_t used = 0;

    if (out == NULL || out_size == 0)
    {
        return;
    }

    out[0] = '\0';
    if (model == NULL || model->entry_count == 0 || !s_transcript_enabled)
    {
        snprintf(out, out_size, "%s", s_transcript_enabled ? "No recent transcript" : "Transcript hidden");
        return;
    }

    for (uint8_t i = 0; i < model->entry_count; ++i)
    {
        const uint8_t index = (uint8_t)((i + s_transcript_page_index) % model->entry_count);
        int written = snprintf(out + used, out_size - used, "%s%s",
                               i == 0 ? "" : "\n", model->entries[index]);
        if (written < 0)
        {
            return;
        }
        if ((size_t)written >= out_size - used)
        {
            out[out_size - 1] = '\0';
            return;
        }
        used += (size_t)written;
    }
}

static uint32_t buddy_ui_prompt_wait_seconds(const buddy_ui_model_t *model)
{
    if (!model->has_prompt || model->prompt_started_ms == 0 ||
        model->uptime_ms < model->prompt_started_ms)
    {
        return 0;
    }

    return (model->uptime_ms - model->prompt_started_ms) / 1000U;
}

static void buddy_ui_update_prompt_state(const buddy_ui_model_t *model)
{
    if (!model->has_prompt)
    {
        s_prompt_id[0] = '\0';
        s_prompt_decision = BUDDY_UI_PROMPT_DECISION_NONE;
        return;
    }

    if (strncmp(s_prompt_id, model->prompt_id, sizeof(s_prompt_id)) != 0)
    {
        strncpy(s_prompt_id, model->prompt_id, sizeof(s_prompt_id) - 1U);
        s_prompt_id[sizeof(s_prompt_id) - 1U] = '\0';
        s_prompt_decision = BUDDY_UI_PROMPT_DECISION_NONE;
    }
}

static bool buddy_ui_update_character_gif(buddy_ui_persona_t persona, uint32_t now_ms)
{
#if LV_USE_GIF
    char path[BUDDY_CHARACTER_RUNTIME_PATH_LEN];
    lv_coord_t frame_w;
    lv_coord_t frame_h;
    lv_coord_t gif_w;
    lv_coord_t gif_h;
    uint16_t zoom = 256;

    if (s_home_character_gif == NULL ||
        !buddy_ui_data_character_get_lvgl_path(buddy_ui_character_state(persona), now_ms,
                                               path, sizeof(path)))
    {
        s_home_character_path[0] = '\0';
        return false;
    }

    buddy_ui_set_hidden(s_home_character_frame, false);
    buddy_ui_set_hidden(s_home_character_gif, false);

    if (strcmp(s_home_character_path, path) != 0)
    {
        lv_gif_set_src(s_home_character_gif, path);
        strncpy(s_home_character_path, path, sizeof(s_home_character_path) - 1U);
        s_home_character_path[sizeof(s_home_character_path) - 1U] = '\0';
    }

    lv_obj_update_layout(s_home_character_frame);
    frame_w = lv_obj_get_width(s_home_character_frame);
    frame_h = lv_obj_get_height(s_home_character_frame);
    gif_w = lv_obj_get_width(s_home_character_gif);
    gif_h = lv_obj_get_height(s_home_character_gif);
    if (frame_w > 0 && frame_h > 0 && gif_w > 0 && gif_h > 0 &&
        (gif_w > frame_w || gif_h > frame_h))
    {
        const uint16_t zoom_w = (uint16_t)((frame_w * 256) / gif_w);
        const uint16_t zoom_h = (uint16_t)((frame_h * 256) / gif_h);
        zoom = zoom_w < zoom_h ? zoom_w : zoom_h;
        if (zoom < 32)
        {
            zoom = 32;
        }
    }
    lv_img_set_zoom(s_home_character_gif, zoom);
    lv_obj_center(s_home_character_gif);
    buddy_ui_set_hidden(s_home_persona_label, true);
    return true;
#else
    (void)persona;
    (void)now_ms;
    return false;
#endif
}

static void buddy_ui_update_character_ascii(const buddy_ui_model_t *model, uint32_t now_ms)
{
    buddy_ascii_frame_t frame;
    const uint8_t species = buddy_ui_effective_ascii_species(model);
    const lv_font_t *ascii_font = BUDDY_UI_FONT_ASCII;

    buddy_ui_set_hidden(s_home_character_frame, true);
    buddy_ui_set_hidden(s_home_character_gif, true);
    buddy_ui_set_hidden(s_home_persona_label, false);

    if (buddy_ascii_render(species, buddy_ui_character_state(model->persona), now_ms, &frame))
    {
        lv_label_set_text(s_home_persona_label, frame.text);
        lv_label_set_long_mode(s_home_persona_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_height(s_home_persona_label, buddy_ui_ascii_label_height(ascii_font));
        lv_obj_set_style_text_font(s_home_persona_label, ascii_font, 0);
        lv_obj_set_style_text_color(s_home_persona_label,
                                    buddy_ui_color_from_rgb565(frame.body_color), 0);
        return;
    }

    lv_label_set_long_mode(s_home_persona_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s_home_persona_label, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(s_home_persona_label, BUDDY_UI_FONT_HERO, 0);
    lv_label_set_text(s_home_persona_label, buddy_ui_persona_text(model->persona));
    lv_obj_set_style_text_color(s_home_persona_label, buddy_ui_persona_color(model->persona), 0);
}

static void buddy_ui_update_common(const buddy_ui_model_t *model, buddy_ui_view_t view)
{
    char buffer[192];
    const char *owner = model->owner[0] != '\0' ? model->owner : model->device_name;

    snprintf(buffer, sizeof(buffer), "%s", owner[0] != '\0' ? owner : "Claude Buddy");
    lv_label_set_text(s_title_label, buffer);
    lv_label_set_text(s_page_label, buddy_ui_screen_name(view));

    switch (view)
    {
    case BUDDY_UI_VIEW_APPROVAL:
        lv_label_set_text(s_nav_label, "KEY1 approve  KEY2 deny");
        break;
    case BUDDY_UI_VIEW_PAIRING:
        lv_label_set_text(s_nav_label, "Enter this passkey on the desktop");
        break;
    case BUDDY_UI_VIEW_SETTINGS:
        lv_label_set_text(s_nav_label, "KEY1 next  KEY2 adjust  hold KEY1 menu");
        break;
    default:
        lv_label_set_text(s_nav_label, "KEY1 next  KEY2 page  hold KEY1 menu");
        break;
    }
}

static void buddy_ui_update_home(const buddy_ui_model_t *model, uint32_t now_ms)
{
    char buffer[192];
    char entries[BUDDY_UI_MAX_ENTRIES * (BUDDY_UI_ENTRY_LEN + 1U)];

    if (buddy_ui_model_prefers_gif(model) &&
        buddy_ui_update_character_gif(model->persona, now_ms))
    {
        lv_obj_set_style_text_font(s_home_persona_label, BUDDY_UI_FONT_HERO, 0);
    }
    else
    {
        buddy_ui_update_character_ascii(model, now_ms);
    }

    snprintf(buffer, sizeof(buffer), "sessions %lu  running %lu  waiting %lu\ntokens today %lu",
             (unsigned long)model->total,
             (unsigned long)model->running,
             (unsigned long)model->waiting,
             (unsigned long)model->tokens_today);
    lv_label_set_text(s_home_summary_label, buffer);

    buddy_ui_join_entries(entries, sizeof(entries), model);
    lv_label_set_text(s_home_entries_label, entries);
}

static void buddy_ui_update_approval(const buddy_ui_model_t *model)
{
    char buffer[192];
    const uint32_t wait_s = buddy_ui_prompt_wait_seconds(model);
    const char *tool = model->prompt_tool[0] != '\0' ? model->prompt_tool : "Tool request";
    const char *hint = model->prompt_hint[0] != '\0' ? model->prompt_hint : "Permission required";

    snprintf(buffer, sizeof(buffer), "%s", tool);
    lv_label_set_text(s_approval_tool_label, buffer);
    lv_label_set_text(s_approval_hint_label, hint);

    snprintf(buffer, sizeof(buffer), "Waiting %lus", (unsigned long)wait_s);
    lv_label_set_text(s_approval_wait_label, buffer);
    lv_obj_set_style_text_color(s_approval_wait_label,
                                wait_s >= BUDDY_UI_MAX_WAIT_WARNING_S ? lv_color_hex(0xFF6B57) :
                                                                         lv_color_hex(0xFFB454),
                                0);

    switch (s_prompt_decision)
    {
    case BUDDY_UI_PROMPT_DECISION_APPROVE_SENT:
        lv_label_set_text(s_approval_status_label, "Approve sent");
        break;
    case BUDDY_UI_PROMPT_DECISION_DENY_SENT:
        lv_label_set_text(s_approval_status_label, "Deny sent");
        break;
    case BUDDY_UI_PROMPT_DECISION_SEND_FAILED:
        lv_label_set_text(s_approval_status_label, "Send failed");
        break;
    case BUDDY_UI_PROMPT_DECISION_NONE:
    default:
        lv_label_set_text(s_approval_status_label, "");
        break;
    }
}

static void buddy_ui_update_pairing(const buddy_ui_model_t *model)
{
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "%06lu", (unsigned long)model->pairing_passkey);
    lv_label_set_text(s_pairing_passkey_label, buffer);
    lv_label_set_text(s_pairing_hint_label, "DisplayOnly bonding passkey");
}

static void buddy_ui_update_pet_stats(const buddy_ui_model_t *model)
{
    char buffer[384];
    const uint32_t nap = model->pet_nap_seconds;

    snprintf(buffer, sizeof(buffer),
             "mood    %u/4\nfed     %u/10\nenergy  %u/5\nlevel   %u\n\n"
             "approved %u\ndenied   %u\nmedian   %us\nnapped   %luh%02lum\n"
             "tokens   %lu",
             (unsigned int)model->pet_mood,
             (unsigned int)model->pet_fed,
             (unsigned int)model->pet_energy,
             (unsigned int)model->pet_level,
             (unsigned int)model->pet_approvals,
             (unsigned int)model->pet_denials,
             (unsigned int)model->pet_velocity_seconds,
             (unsigned long)(nap / 3600U),
             (unsigned long)((nap / 60U) % 60U),
             (unsigned long)model->pet_tokens);
    lv_label_set_text(s_pet_stats_label, buffer);
}

static void buddy_ui_update_info(const buddy_ui_model_t *model)
{
    char buffer[384];

    if (s_info_page_index == 0)
    {
        snprintf(buffer, sizeof(buffer),
                 "Claude\nname: %s\nowner: %s\n\nDevice\nBLE: %s\nsecure: %s\nuptime: %lus",
                 model->device_name[0] != '\0' ? model->device_name : "Claude Buddy",
                 model->owner[0] != '\0' ? model->owner : "unset",
                 model->connected ? "connected" : "offline",
                 model->encrypted ? "encrypted" : "not encrypted",
                 (unsigned long)(model->uptime_ms / 1000U));
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                 "Runtime\nrx lines: %lu\nrx overflow: %lu\nlast snapshot: %lus\nprompt: %s",
                 (unsigned long)model->rx_lines,
                 (unsigned long)model->rx_overflowed,
                 (unsigned long)(model->last_snapshot_ms / 1000U),
                 model->has_prompt ? model->prompt_id : "none");
    }

    lv_label_set_text(s_info_label, buffer);
}

static void buddy_ui_set_switch(lv_obj_t *sw, bool enabled)
{
    if (enabled)
    {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_clear_state(sw, LV_STATE_CHECKED);
    }
}

static void buddy_ui_update_settings(const buddy_ui_model_t *model)
{
    char buffer[96];
    const char *focus;
    const bool gif_available = buddy_ui_data_character_available();
    const uint8_t ascii_species = buddy_ui_effective_ascii_species(model);
    const uint8_t species_count = model->species_count > 0 ? model->species_count : buddy_ascii_species_count();
    const uint8_t total = (uint8_t)(species_count + (gif_available ? 1U : 0U));
    const uint8_t pos = (buddy_ui_model_prefers_gif(model) && gif_available) ?
                            total :
                            (uint8_t)(ascii_species + 1U);

    snprintf(buffer, sizeof(buffer), "Brightness %u%%", (unsigned int)s_brightness);
    lv_label_set_text(s_settings_brightness_label, buffer);
    lv_bar_set_value(s_settings_brightness_bar, s_brightness, LV_ANIM_OFF);
    buddy_ui_set_switch(s_settings_sound_switch, s_sound_enabled);
    buddy_ui_set_switch(s_settings_led_switch, s_led_enabled);
    buddy_ui_set_switch(s_settings_transcript_switch, s_transcript_enabled);
    if (buddy_ui_model_prefers_gif(model) && gif_available)
    {
        snprintf(buffer, sizeof(buffer), "Pet GIF %u/%u", (unsigned int)pos, (unsigned int)total);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Pet %s %u/%u",
                 buddy_ascii_species_name(ascii_species),
                 (unsigned int)pos,
                 (unsigned int)total);
    }
    lv_label_set_text(s_settings_pet_label, buffer);
    lv_label_set_text(s_settings_reset_label, s_reset_armed ? "Reset armed" : "Reset");

    switch (s_settings_index)
    {
    case 0:
        focus = "Next: brightness";
        break;
    case 1:
        focus = "Next: sound";
        break;
    case 2:
        focus = "Next: LED";
        break;
    case 3:
        focus = "Next: transcript";
        break;
    case 4:
        focus = "Next: pet";
        break;
    case 5:
    default:
        focus = "Next: reset confirm";
        break;
    }
    lv_label_set_text(s_settings_focus_label, focus);
}

static lv_obj_t *buddy_ui_settings_row(lv_obj_t *parent, const char *text)
{
    buddy_ui_metrics_t metrics = buddy_ui_metrics();
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_t *label;

    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 42);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1D2630), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, metrics.radius, 0);
    lv_obj_set_style_pad_all(row, metrics.space_sm, 0);

    label = buddy_ui_label(row, BUDDY_UI_FONT_SMALL, lv_color_hex(0xE1E6EF));
    lv_label_set_text(label, text);
    lv_obj_set_flex_grow(label, 1);
    return row;
}

static void buddy_ui_toggle_setting_event(lv_event_t *event)
{
    uintptr_t index;
    buddy_ui_model_t model;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
        return;
    }

    index = (uintptr_t)lv_event_get_user_data(event);
    switch (index)
    {
    case 0:
        s_brightness = (uint8_t)(s_brightness >= 100 ? 20 : s_brightness + 20);
        break;
    case 1:
        s_sound_enabled = !s_sound_enabled;
        break;
    case 2:
        s_led_enabled = !s_led_enabled;
        break;
    case 3:
        s_transcript_enabled = !s_transcript_enabled;
        break;
    case 4:
        if (buddy_ui_data_get_model(&model))
        {
            buddy_ui_cycle_pet(&model);
        }
        break;
    case 5:
    default:
        if (s_reset_armed)
        {
            buddy_ui_run_factory_reset();
        }
        else
        {
            s_reset_armed = true;
        }
        break;
    }
    if (index < 4U)
    {
        buddy_ui_save_settings();
    }
    buddy_ui_post_action(BUDDY_UI_ACTION_MENU);
}

static void buddy_ui_create_screen(void)
{
    buddy_ui_metrics_t metrics = buddy_ui_metrics();
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *button_row;
    lv_obj_t *row;

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101419), 0);
    lv_obj_set_style_pad_all(screen, metrics.space_lg, 0);
    lv_obj_set_style_pad_row(screen, metrics.space_md, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    s_header_row = lv_obj_create(screen);
    lv_obj_set_width(s_header_row, lv_pct(100));
    lv_obj_set_height(s_header_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_header_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(s_header_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_header_row, 0, 0);
    lv_obj_set_style_pad_all(s_header_row, 0, 0);
    lv_obj_set_style_pad_left(s_header_row, metrics.space_lg, 0);
    lv_obj_set_style_pad_right(s_header_row, metrics.space_lg, 0);
    lv_obj_set_style_pad_column(s_header_row, metrics.space_md, 0);

    s_title_label = buddy_ui_label(s_header_row, BUDDY_UI_FONT_TITLE, lv_color_hex(0xF3F6FA));
    lv_obj_set_width(s_title_label, 0);
    lv_obj_set_flex_grow(s_title_label, 1);
    lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_DOT);
    s_page_label = buddy_ui_label(s_header_row, BUDDY_UI_FONT_SMALL, lv_color_hex(0xFFB454));
    lv_obj_set_width(s_page_label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(s_page_label, LV_LABEL_LONG_CLIP);

    s_home_page = buddy_ui_page(screen);
    lv_obj_set_style_pad_row(s_home_page, metrics.space_sm, 0);
    s_home_status_row = lv_obj_create(s_home_page);
    lv_obj_set_width(s_home_status_row, lv_pct(100));
    lv_obj_set_height(s_home_status_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_home_status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_home_status_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(s_home_status_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_home_status_row, 0, 0);
    lv_obj_set_style_pad_all(s_home_status_row, 0, 0);
    lv_obj_set_style_pad_column(s_home_status_row, metrics.space_md, 0);

    s_home_character_frame = lv_obj_create(s_home_status_row);
    lv_obj_set_width(s_home_character_frame, lv_pct(40));
    lv_obj_set_height(s_home_character_frame, buddy_ui_character_frame_height());
    lv_obj_set_style_bg_opa(s_home_character_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_home_character_frame, 0, 0);
    lv_obj_set_style_pad_all(s_home_character_frame, 0, 0);
    lv_obj_clear_flag(s_home_character_frame, LV_OBJ_FLAG_SCROLLABLE);
#if LV_USE_GIF
    s_home_character_gif = lv_gif_create(s_home_character_frame);
    lv_obj_center(s_home_character_gif);
#endif
    buddy_ui_set_hidden(s_home_character_frame, true);
    s_home_persona_label = buddy_ui_label(s_home_status_row, BUDDY_UI_FONT_HERO, lv_color_hex(0xB6BCC8));
    lv_obj_set_width(s_home_persona_label, lv_pct(40));
    lv_obj_set_style_text_align(s_home_persona_label, LV_TEXT_ALIGN_CENTER, 0);
    s_home_summary_label = buddy_ui_label(s_home_status_row, BUDDY_UI_FONT_BODY, lv_color_hex(0xE1E6EF));
    lv_obj_set_width(s_home_summary_label, 0);
    lv_obj_set_flex_grow(s_home_summary_label, 1);
    s_home_entries_label = buddy_ui_label(s_home_page, BUDDY_UI_FONT_SMALL, lv_color_hex(0xB6BCC8));
    lv_obj_set_flex_grow(s_home_entries_label, 1);

    s_pet_page = buddy_ui_page(screen);
    s_pet_stats_label = buddy_ui_label(s_pet_page, BUDDY_UI_FONT_BODY, lv_color_hex(0xE1E6EF));
    lv_obj_set_flex_grow(s_pet_stats_label, 1);

    s_approval_page = buddy_ui_page(screen);
    s_approval_tool_label = buddy_ui_label(s_approval_page, BUDDY_UI_FONT_TITLE, lv_color_hex(0xF3F6FA));
    s_approval_hint_label = buddy_ui_label(s_approval_page, BUDDY_UI_FONT_BODY, lv_color_hex(0xE1E6EF));
    s_approval_wait_label = buddy_ui_label(s_approval_page, BUDDY_UI_FONT_BODY, lv_color_hex(0xFFB454));
    s_approval_status_label = buddy_ui_label(s_approval_page, BUDDY_UI_FONT_SMALL, lv_color_hex(0xA8E6A1));
    button_row = lv_obj_create(s_approval_page);
    lv_obj_set_width(button_row, lv_pct(100));
    lv_obj_set_height(button_row, 52);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(button_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(button_row, 0, 0);
    lv_obj_set_style_pad_all(button_row, 0, 0);
    lv_obj_set_style_pad_column(button_row, metrics.space_md, 0);
    buddy_ui_button(button_row, "Approve", lv_color_hex(0x2F9E6D), BUDDY_UI_ACTION_APPROVE);
    buddy_ui_button(button_row, "Deny", lv_color_hex(0x9E3A35), BUDDY_UI_ACTION_DENY);

    s_pairing_page = buddy_ui_page(screen);
    s_pairing_passkey_label = buddy_ui_label(s_pairing_page, BUDDY_UI_FONT_HERO, lv_color_hex(0xF3F6FA));
    lv_obj_set_style_text_align(s_pairing_passkey_label, LV_TEXT_ALIGN_CENTER, 0);
    s_pairing_hint_label = buddy_ui_label(s_pairing_page, BUDDY_UI_FONT_BODY, lv_color_hex(0xB6BCC8));
    lv_obj_set_style_text_align(s_pairing_hint_label, LV_TEXT_ALIGN_CENTER, 0);

    s_info_page = buddy_ui_page(screen);
    s_info_label = buddy_ui_label(s_info_page, BUDDY_UI_FONT_BODY, lv_color_hex(0xE1E6EF));

    s_settings_page = buddy_ui_page(screen);
    row = buddy_ui_settings_row(s_settings_page, "");
    s_settings_brightness_label = lv_obj_get_child(row, 0);
    s_settings_brightness_bar = lv_bar_create(row);
    lv_obj_set_width(s_settings_brightness_bar, 96);
    lv_bar_set_range(s_settings_brightness_bar, 0, 100);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)0);

    row = buddy_ui_settings_row(s_settings_page, "Sound");
    s_settings_sound_switch = lv_switch_create(row);
    lv_obj_add_event_cb(row, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)1);
    lv_obj_add_event_cb(s_settings_sound_switch, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)1);

    row = buddy_ui_settings_row(s_settings_page, "LED");
    s_settings_led_switch = lv_switch_create(row);
    lv_obj_add_event_cb(row, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)2);
    lv_obj_add_event_cb(s_settings_led_switch, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)2);

    row = buddy_ui_settings_row(s_settings_page, "Transcript");
    s_settings_transcript_switch = lv_switch_create(row);
    lv_obj_add_event_cb(row, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)3);
    lv_obj_add_event_cb(s_settings_transcript_switch, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)3);

    row = buddy_ui_settings_row(s_settings_page, "");
    s_settings_pet_label = lv_obj_get_child(row, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)4);

    row = buddy_ui_settings_row(s_settings_page, "");
    s_settings_reset_label = lv_obj_get_child(row, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, buddy_ui_toggle_setting_event, LV_EVENT_CLICKED, (void *)5);

    s_settings_focus_label = buddy_ui_label(s_settings_page, BUDDY_UI_FONT_SMALL, lv_color_hex(0xFFB454));

    s_nav_label = buddy_ui_label(screen, BUDDY_UI_FONT_SMALL, lv_color_hex(0x8A93A3));
    lv_obj_set_style_text_align(s_nav_label, LV_TEXT_ALIGN_CENTER, 0);
}

static void buddy_ui_show_view(buddy_ui_view_t view)
{
    s_view = view;
    buddy_ui_set_hidden(s_home_page, view != BUDDY_UI_VIEW_HOME);
    buddy_ui_set_hidden(s_pet_page, view != BUDDY_UI_VIEW_PET_STATS);
    buddy_ui_set_hidden(s_approval_page, view != BUDDY_UI_VIEW_APPROVAL);
    buddy_ui_set_hidden(s_pairing_page, view != BUDDY_UI_VIEW_PAIRING);
    buddy_ui_set_hidden(s_info_page, view != BUDDY_UI_VIEW_INFO);
    buddy_ui_set_hidden(s_settings_page, view != BUDDY_UI_VIEW_SETTINGS);
}

static void buddy_ui_refresh(bool force)
{
    buddy_ui_model_t model;
    uint32_t now = (uint32_t)rt_tick_get_millisecond();
    bool action_changed;
    buddy_ui_view_t view;

    if (!s_ready)
    {
        return;
    }

    if (!buddy_ui_data_get_model(&model))
    {
        return;
    }

    buddy_ui_update_prompt_state(&model);
    action_changed = buddy_ui_process_actions(&model);

    if (!force && !action_changed && now - s_last_refresh_ms < BUDDY_UI_REFRESH_MS)
    {
        return;
    }
    s_last_refresh_ms = now;

    if (action_changed)
    {
        buddy_ui_data_get_model(&model);
        buddy_ui_update_prompt_state(&model);
    }

    view = buddy_ui_effective_view(&model);
    if (view != s_view)
    {
        buddy_ui_log_home_approval_transition(s_view, view);
        buddy_ui_show_view(view);
    }

    buddy_ui_update_common(&model, view);
    switch (view)
    {
    case BUDDY_UI_VIEW_PET_STATS:
        buddy_ui_update_pet_stats(&model);
        break;
    case BUDDY_UI_VIEW_APPROVAL:
        buddy_ui_update_approval(&model);
        break;
    case BUDDY_UI_VIEW_PAIRING:
        buddy_ui_update_pairing(&model);
        break;
    case BUDDY_UI_VIEW_INFO:
        buddy_ui_update_info(&model);
        break;
    case BUDDY_UI_VIEW_SETTINGS:
        buddy_ui_update_settings(&model);
        break;
    case BUDDY_UI_VIEW_HOME:
    default:
        buddy_ui_update_home(&model, now);
        break;
    }
}

#ifdef USING_BUTTON_LIB
static button_active_state_t buddy_ui_key1_active_state(void)
{
#ifdef BSP_KEY1_ACTIVE_HIGH
    return BUTTON_ACTIVE_HIGH;
#else
    return BUTTON_ACTIVE_LOW;
#endif
}

static button_active_state_t buddy_ui_key2_active_state(void)
{
#ifdef BSP_KEY2_ACTIVE_HIGH
    return BUTTON_ACTIVE_HIGH;
#else
    return BUTTON_ACTIVE_LOW;
#endif
}

static void buddy_ui_button_handler(int32_t pin, button_action_t action)
{
    if (action == BUTTON_CLICKED)
    {
        if (pin == BSP_KEY1_PIN)
        {
            buddy_ui_post_action(BUDDY_UI_ACTION_PRIMARY);
        }
        else if (pin == BSP_KEY2_PIN)
        {
            buddy_ui_post_action(BUDDY_UI_ACTION_SECONDARY);
        }
    }
    else if (action == BUTTON_LONG_PRESSED && pin == BSP_KEY1_PIN)
    {
        buddy_ui_post_action(BUDDY_UI_ACTION_MENU);
    }
}

static int buddy_ui_button_init_one(int32_t pin,
                                    button_active_state_t active_state,
                                    const char *name,
                                    int32_t *out_id)
{
    button_cfg_t cfg;
    int32_t id;

    memset(&cfg, 0, sizeof(cfg));
    cfg.pin = pin;
    cfg.active_state = active_state;
    cfg.mode = PIN_MODE_INPUT;
    cfg.button_handler = buddy_ui_button_handler;
    cfg.debounce_time = 30;
    cfg.name = name;

    id = button_init(&cfg);
    if (id < 0)
    {
        rt_kprintf("Buddy UI button init failed pin=%d err=%d\n", pin, id);
        return RT_ERROR;
    }

    if (button_enable(id) != SF_EOK)
    {
        rt_kprintf("Buddy UI button enable failed pin=%d id=%d\n", pin, id);
        return RT_ERROR;
    }

    *out_id = id;
    return RT_EOK;
}

static int buddy_ui_buttons_init(void)
{
    int ret;

    ret = buddy_ui_button_init_one(BSP_KEY1_PIN, buddy_ui_key1_active_state(), "buddy-key1", &s_key1_id);
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = buddy_ui_button_init_one(BSP_KEY2_PIN, buddy_ui_key2_active_state(), "buddy-key2", &s_key2_id);
    if (ret != RT_EOK)
    {
        return ret;
    }

    rt_kprintf("Buddy UI keys ready: KEY1=%d KEY2=%d\n", BSP_KEY1_PIN, BSP_KEY2_PIN);
    return RT_EOK;
}
#else
static int buddy_ui_buttons_init(void)
{
    rt_kprintf("Buddy UI buttons disabled: USING_BUTTON_LIB is not set\n");
    return RT_EOK;
}
#endif

int buddy_ui_init(void)
{
    rt_err_t ret = littlevgl2rtt_init("lcd");
    if (ret != RT_EOK)
    {
        return ret;
    }

    lv_ex_data_pool_init();

    ret = buddy_ui_buttons_init();
    if (ret != RT_EOK)
    {
        return ret;
    }

    buddy_ui_load_settings();
    buddy_ui_create_screen();
    buddy_ui_show_view(BUDDY_UI_VIEW_HOME);
    s_ready = true;
    buddy_ui_refresh(true);
    return RT_EOK;
}

uint32_t buddy_ui_run_once(void)
{
    uint32_t delay_ms;

    buddy_ui_refresh(false);
    delay_ms = (uint32_t)lv_task_handler();
    if (delay_ms == 0 || delay_ms > BUDDY_UI_MAX_HANDLER_DELAY_MS)
    {
        delay_ms = BUDDY_UI_MAX_HANDLER_DELAY_MS;
    }

    return delay_ms;
}
