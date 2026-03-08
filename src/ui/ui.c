/*
 * UI Implementation - Modern connected watch dashboard
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>

#define THEME_COUNT 4

struct theme_palette {
    uint32_t accent;
    uint32_t tint;
    const char *name;
};

static const struct theme_palette theme_palettes[THEME_COUNT] = {
    {0x64D8FF, 0x102733, "Cyan"},
    {0x7EDC8B, 0x142A1A, "Green"},
    {0xFFB45E, 0x2F2113, "Orange"},
    {0xF25CFF, 0x2B1530, "Magenta"},
};

/* --- Color Helper (SPI BGR fix for ILI9341) --- */
static inline uint32_t fix_color(uint32_t color)
{
    return ((color & 0xFF0000) >> 16) |
           (color & 0x00FF00) |
           ((color & 0x0000FF) << 16);
}

static inline lv_color_t color_hex(uint32_t color)
{
    return lv_color_hex(fix_color(color));
}

/* --- Theme & Styles --- */
static lv_style_t style_card;
static lv_style_t style_title;
static lv_style_t style_muted;

static uint8_t theme_index;
static bool dark_overlay_enabled;
static bool ble_connected;
static uint32_t uptime_seconds;

/* --- Screens --- */
static lv_obj_t *screen_sensors;
static lv_obj_t *screen_clock;
static lv_obj_t *screen_compass;
static lv_obj_t *screen_pedometer;
static lv_obj_t *overlay_layer;
static lv_obj_t *status_modal;
static lv_obj_t *sleep_overlay;
static lv_obj_t *status_title;
static lv_obj_t *status_label;

/* Circular navigation: sensors(0) -> clock(1) -> compass(2) -> pedometer(3) */
static int current_screen_idx;

/* --- Decorative / Themed Panels --- */
static lv_obj_t *sensor_card_env;
static lv_obj_t *sensor_card_motion;
static lv_obj_t *sensor_badge;
static lv_obj_t *sensor_badge_label;
static lv_obj_t *datetime_pill;

static lv_obj_t *clock_panel;
static lv_obj_t *calendar_panel;

static lv_obj_t *compass_dial;
static lv_obj_t *compass_ring;
static lv_obj_t *compass_needle_north;
static lv_obj_t *compass_needle_south;
static lv_obj_t *compass_center_dot;
static lv_obj_t *compass_mark_n;
static lv_obj_t *compass_mark_e;
static lv_obj_t *compass_mark_s;
static lv_obj_t *compass_mark_w;

static lv_obj_t *pedometer_panel;
static lv_obj_t *goal_pill;

/* --- Sensor Screen Widgets --- */
static lv_obj_t *label_temp_val;
static lv_obj_t *label_hum_val;
static lv_obj_t *bar_hum;
static lv_obj_t *label_press_val;
static lv_obj_t *label_accel_val;
static lv_obj_t *label_gyro_val;
static lv_obj_t *label_mag_val;
static lv_obj_t *label_datetime_small;

/* --- Clock Screen Widgets --- */
static lv_obj_t *arc_seconds;
static lv_obj_t *label_big_time;
static lv_obj_t *label_date;
static lv_obj_t *calendar;

/* --- Compass Screen Widgets --- */
static lv_obj_t *label_heading;

/* --- Pedometer Screen Widgets --- */
static lv_obj_t *arc_steps;
static lv_obj_t *label_step_count;
static lv_obj_t *label_step_goal;

/* --- Text Buffers --- */
static char datetime_text[32];
static char date_text[16];
static char status_text[128];
static lv_point_precise_t compass_needle_north_points[2];
static lv_point_precise_t compass_needle_south_points[2];

static const struct theme_palette *get_theme_palette(void)
{
    return &theme_palettes[theme_index];
}

static lv_color_t get_accent_color(void)
{
    return color_hex(get_theme_palette()->accent);
}

static lv_color_t get_tint_color(void)
{
    return color_hex(get_theme_palette()->tint);
}

static const char *get_theme_name(void)
{
    return get_theme_palette()->name;
}

/* --- Touch callbacks (FT6206 touchscreen) --- */
static void touch_switch_screen_cb(lv_event_t *e)
{
    (void)e;
    ui_switch_screen();
}

static void touch_toggle_calendar_cb(lv_event_t *e)
{
    (void)e;

    if (!calendar_panel) {
        return;
    }

    if (lv_obj_has_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void touch_close_popup_cb(lv_event_t *e)
{
    (void)e;
    if (!lv_obj_has_flag(status_modal, LV_OBJ_FLAG_HIDDEN)) {
        ui_toggle_status_popup();
    }
}

/* --- Shared UI Helpers --- */
static void init_styles(void)
{
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, color_hex(0x11161D));
    lv_style_set_bg_grad_color(&style_card, color_hex(0x090C10));
    lv_style_set_bg_grad_dir(&style_card, LV_GRAD_DIR_VER);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 18);
    lv_style_set_pad_all(&style_card, 12);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, color_hex(0x28313B));
    lv_style_set_border_opa(&style_card, LV_OPA_60);
    lv_style_set_shadow_width(&style_card, 18);
    lv_style_set_shadow_ofs_y(&style_card, 6);
    lv_style_set_shadow_color(&style_card, color_hex(0x000000));
    lv_style_set_shadow_opa(&style_card, LV_OPA_20);

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_title, color_hex(0x97A5B5));

    lv_style_init(&style_muted);
    lv_style_set_text_font(&style_muted, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_muted, color_hex(0xC8D0D9));
    lv_style_set_text_opa(&style_muted, LV_OPA_70);
}

static void prepare_screen(lv_obj_t *screen, uint32_t top_color, uint32_t bottom_color)
{
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, color_hex(top_color), 0);
    lv_obj_set_style_bg_grad_color(screen, color_hex(bottom_color), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_radius(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
}

static void add_backdrop_blob(lv_obj_t *parent, lv_coord_t size,
                              lv_coord_t x, lv_coord_t y,
                              uint32_t color, lv_opa_t opa)
{
    lv_obj_t *blob = lv_obj_create(parent);
    lv_obj_clear_flag(blob, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(blob, size, size);
    lv_obj_set_pos(blob, x, y);
    lv_obj_set_style_radius(blob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(blob, color_hex(color), 0);
    lv_obj_set_style_bg_opa(blob, opa, 0);
    lv_obj_set_style_border_width(blob, 0, 0);
    lv_obj_set_style_shadow_width(blob, 0, 0);
}

static lv_obj_t *create_panel(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_add_style(panel, &style_card, 0);
    lv_obj_set_size(panel, w, h);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static lv_obj_t *create_card(lv_obj_t *parent, const char *title, int w, int h)
{
    lv_obj_t *card = create_panel(parent, w, h);
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_add_style(lbl, &style_title, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    return card;
}

static lv_obj_t *create_pill(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pill, w, h);
    lv_obj_set_style_radius(pill, 16, 0);
    lv_obj_set_style_bg_color(pill, color_hex(0x0F141A), 0);
    lv_obj_set_style_bg_grad_color(pill, color_hex(0x090C10), 0);
    lv_obj_set_style_bg_grad_dir(pill, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, color_hex(0x28313B), 0);
    lv_obj_set_style_pad_hor(pill, 12, 0);
    lv_obj_set_style_pad_ver(pill, 4, 0);
    lv_obj_set_style_shadow_width(pill, 12, 0);
    lv_obj_set_style_shadow_ofs_y(pill, 4, 0);
    lv_obj_set_style_shadow_color(pill, color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(pill, LV_OPA_20, 0);
    return pill;
}

static void style_arc_widget(lv_obj_t *arc, lv_coord_t size)
{
    lv_obj_set_size(arc, size, size);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color_hex(0x24303A), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_70, LV_PART_MAIN);
}

static lv_obj_t *create_compass_tick(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                                     lv_coord_t x, lv_coord_t y, lv_opa_t opa)
{
    lv_obj_t *tick = lv_obj_create(parent);
    lv_obj_clear_flag(tick, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tick, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(tick, w, h);
    lv_obj_set_pos(tick, x, y);
    lv_obj_set_style_radius(tick, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tick, color_hex(0xAAB6C2), 0);
    lv_obj_set_style_bg_opa(tick, opa, 0);
    lv_obj_set_style_border_width(tick, 0, 0);
    lv_obj_set_style_shadow_width(tick, 0, 0);
    return tick;
}

static void apply_theme(void)
{
    lv_color_t accent = get_accent_color();
    lv_color_t tint = get_tint_color();

    if (sensor_badge) {
        lv_obj_set_style_bg_color(sensor_badge, tint, 0);
        lv_obj_set_style_border_color(sensor_badge, accent, 0);
    }
    if (sensor_badge_label) {
        lv_obj_set_style_text_color(sensor_badge_label, accent, 0);
    }
    if (sensor_card_env) {
        lv_obj_set_style_border_color(sensor_card_env, accent, 0);
        lv_obj_set_style_shadow_color(sensor_card_env, accent, 0);
    }
    if (sensor_card_motion) {
        lv_obj_set_style_border_color(sensor_card_motion, accent, 0);
    }
    if (datetime_pill) {
        lv_obj_set_style_bg_color(datetime_pill, tint, 0);
        lv_obj_set_style_border_color(datetime_pill, accent, 0);
    }
    if (clock_panel) {
        lv_obj_set_style_border_color(clock_panel, accent, 0);
    }
    if (calendar_panel) {
        lv_obj_set_style_bg_color(calendar_panel, tint, 0);
        lv_obj_set_style_bg_grad_color(calendar_panel, color_hex(0x0A0D11), 0);
        lv_obj_set_style_border_color(calendar_panel, accent, 0);
    }
    if (compass_dial) {
        lv_obj_set_style_border_color(compass_dial, accent, 0);
    }
    if (compass_ring) {
        lv_obj_set_style_border_color(compass_ring, accent, 0);
        lv_obj_set_style_bg_color(compass_ring, tint, 0);
    }
    if (compass_needle_north) {
        lv_obj_set_style_line_color(compass_needle_north, accent, 0);
    }
    if (compass_center_dot) {
        lv_obj_set_style_border_color(compass_center_dot, accent, 0);
        lv_obj_set_style_bg_color(compass_center_dot, tint, 0);
    }
    if (pedometer_panel) {
        lv_obj_set_style_border_color(pedometer_panel, accent, 0);
    }
    if (goal_pill) {
        lv_obj_set_style_bg_color(goal_pill, tint, 0);
        lv_obj_set_style_bg_grad_color(goal_pill, color_hex(0x0A0D11), 0);
        lv_obj_set_style_border_color(goal_pill, accent, 0);
    }
    if (status_modal) {
        lv_obj_set_style_border_color(status_modal, accent, 0);
        lv_obj_set_style_shadow_color(status_modal, accent, 0);
    }
    if (status_title) {
        lv_obj_set_style_text_color(status_title, accent, 0);
    }
    if (label_temp_val) {
        lv_obj_set_style_text_color(label_temp_val, accent, 0);
    }
    if (bar_hum) {
        lv_obj_set_style_bg_color(bar_hum, accent, LV_PART_INDICATOR);
    }
    if (label_datetime_small) {
        lv_obj_set_style_text_color(label_datetime_small, accent, 0);
    }
    if (arc_seconds) {
        lv_obj_set_style_arc_color(arc_seconds, accent, LV_PART_INDICATOR);
    }
    if (label_date) {
        lv_obj_set_style_text_color(label_date, accent, 0);
    }
    if (label_heading) {
        lv_obj_set_style_text_color(label_heading, accent, 0);
    }
    if (compass_mark_n) {
        lv_obj_set_style_text_color(compass_mark_n, accent, 0);
    }
    if (compass_mark_e) {
        lv_obj_set_style_text_color(compass_mark_e, color_hex(0x91A0AF), 0);
    }
    if (compass_mark_s) {
        lv_obj_set_style_text_color(compass_mark_s, color_hex(0x91A0AF), 0);
    }
    if (compass_mark_w) {
        lv_obj_set_style_text_color(compass_mark_w, color_hex(0x91A0AF), 0);
    }
    if (arc_steps) {
        lv_obj_set_style_arc_color(arc_steps, accent, LV_PART_INDICATOR);
    }
    if (label_step_goal) {
        lv_obj_set_style_text_color(label_step_goal, accent, 0);
    }
}

/* --- Build Sensor Screen --- */
static void build_sensor_screen(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t hor = disp ? lv_disp_get_hor_res(disp) : 320;

    screen_sensors = lv_obj_create(NULL);
    prepare_screen(screen_sensors, 0x07121B, 0x020508);
    add_backdrop_blob(screen_sensors, 170, -60, -55, 0x143240, LV_OPA_50);
    add_backdrop_blob(screen_sensors, 150, 225, 140, 0x0D2432, LV_OPA_40);

    lv_obj_t *header = lv_label_create(screen_sensors);
    lv_label_set_text(header, "CONNECTED WATCH");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(header, color_hex(0xFFFFFF), 0);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 14, 10);
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    sensor_badge = create_pill(screen_sensors, 86, 24);
    lv_obj_align(sensor_badge, LV_ALIGN_TOP_RIGHT, -14, 10);
    sensor_badge_label = lv_label_create(sensor_badge);
    lv_label_set_text(sensor_badge_label, "SENSORS");
    lv_obj_set_style_text_font(sensor_badge_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sensor_badge_label, LV_ALIGN_CENTER, 0, 0);

    sensor_card_env = create_card(screen_sensors, "ENVIRONMENT", hor - 24, 82);
    lv_obj_align(sensor_card_env, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_add_flag(sensor_card_env, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sensor_card_env, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    label_temp_val = lv_label_create(sensor_card_env);
    lv_label_set_text(label_temp_val, "--.- C");
    lv_obj_set_style_text_font(label_temp_val, &lv_font_montserrat_28, 0);
    lv_obj_align(label_temp_val, LV_ALIGN_TOP_LEFT, 0, 18);

    label_hum_val = lv_label_create(sensor_card_env);
    lv_label_set_text(label_hum_val, "HUM  --%");
    lv_obj_set_style_text_font(label_hum_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_hum_val, color_hex(0xEAF1F7), 0);
    lv_obj_align(label_hum_val, LV_ALIGN_TOP_RIGHT, 0, 20);

    bar_hum = lv_bar_create(sensor_card_env);
    lv_obj_set_size(bar_hum, 106, 8);
    lv_obj_align(bar_hum, LV_ALIGN_RIGHT_MID, 0, 10);
    lv_bar_set_range(bar_hum, 0, 100);
    lv_bar_set_value(bar_hum, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_hum, color_hex(0x31414C), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_hum, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_hum, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_hum, 6, LV_PART_INDICATOR);

    label_press_val = lv_label_create(sensor_card_env);
    lv_label_set_text(label_press_val, "PRESSURE  ---- hPa");
    lv_obj_add_style(label_press_val, &style_muted, 0);
    lv_obj_align(label_press_val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    sensor_card_motion = create_card(screen_sensors, "MOTION", hor - 24, 88);
    lv_obj_align(sensor_card_motion, LV_ALIGN_TOP_MID, 0, 128);
    lv_obj_add_flag(sensor_card_motion, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sensor_card_motion, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    label_accel_val = lv_label_create(sensor_card_motion);
    lv_label_set_text(label_accel_val, "ACC  --   --   --");
    lv_obj_set_style_text_font(label_accel_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_accel_val, color_hex(0xF5F7FA), 0);
    lv_obj_align(label_accel_val, LV_ALIGN_TOP_LEFT, 0, 18);

    label_gyro_val = lv_label_create(sensor_card_motion);
    lv_label_set_text(label_gyro_val, "GYR  --   --   --");
    lv_obj_add_style(label_gyro_val, &style_muted, 0);
    lv_obj_align(label_gyro_val, LV_ALIGN_LEFT_MID, 0, 4);

    label_mag_val = lv_label_create(sensor_card_motion);
    lv_label_set_text(label_mag_val, "MAG  --   --   --");
    lv_obj_add_style(label_mag_val, &style_muted, 0);
    lv_obj_align(label_mag_val, LV_ALIGN_BOTTOM_LEFT, 0, -2);

    datetime_pill = create_pill(screen_sensors, hor - 24, 26);
    lv_obj_align(datetime_pill, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_flag(datetime_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(datetime_pill, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    label_datetime_small = lv_label_create(datetime_pill);
    lv_label_set_text(label_datetime_small, "--/--/----  --:--:--");
    lv_obj_set_style_text_font(label_datetime_small, &lv_font_montserrat_14, 0);
    lv_obj_align(label_datetime_small, LV_ALIGN_CENTER, 0, 0);
}

/* --- Build Clock Screen --- */
static void build_clock_screen(void)
{
    screen_clock = lv_obj_create(NULL);
    prepare_screen(screen_clock, 0x140A18, 0x040507);
    add_backdrop_blob(screen_clock, 165, 210, -55, 0x382148, LV_OPA_40);
    add_backdrop_blob(screen_clock, 140, -45, 165, 0x33220E, LV_OPA_30);

    lv_obj_t *title = lv_label_create(screen_clock);
    lv_label_set_text(title, "TIME");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hint = lv_label_create(screen_clock);
    lv_label_set_text(hint, "tap date to open calendar");
    lv_obj_add_style(hint, &style_muted, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 30);

    arc_seconds = lv_arc_create(screen_clock);
    style_arc_widget(arc_seconds, 188);
    lv_arc_set_range(arc_seconds, 0, 60);
    lv_arc_set_value(arc_seconds, 0);
    lv_obj_align(arc_seconds, LV_ALIGN_CENTER, 0, -18);

    clock_panel = create_panel(screen_clock, 176, 92);
    lv_obj_align(clock_panel, LV_ALIGN_CENTER, 0, -18);

    label_big_time = lv_label_create(clock_panel);
    lv_label_set_text(label_big_time, "--:--:--");
    lv_obj_set_style_text_font(label_big_time, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_big_time, color_hex(0xFFFFFF), 0);
    lv_obj_align(label_big_time, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_flag(label_big_time, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_big_time, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    label_date = lv_label_create(clock_panel);
    lv_label_set_text(label_date, "--/--/----");
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_align(label_date, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_flag(label_date, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_date, touch_toggle_calendar_cb, LV_EVENT_CLICKED, NULL);

    calendar_panel = create_panel(screen_clock, 212, 104);
    lv_obj_align(calendar_panel, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN);

    calendar = lv_calendar_create(calendar_panel);
    lv_obj_set_size(calendar, 188, 82);
    lv_obj_align(calendar, LV_ALIGN_CENTER, 0, 6);
    lv_calendar_set_today_date(calendar, 1970, 1, 1);
    lv_calendar_set_showed_date(calendar, 1970, 1);
}

/* --- Build Compass Screen --- */
static void build_compass_screen(void)
{
    screen_compass = lv_obj_create(NULL);
    prepare_screen(screen_compass, 0x07150E, 0x030607);
    add_backdrop_blob(screen_compass, 160, -60, -50, 0x16332A, LV_OPA_40);
    add_backdrop_blob(screen_compass, 140, 235, 150, 0x0E2430, LV_OPA_30);

    lv_obj_t *title = lv_label_create(screen_compass);
    lv_label_set_text(title, "COMPASS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    compass_dial = lv_obj_create(screen_compass);
    lv_obj_clear_flag(compass_dial, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(compass_dial, 176, 176);
    lv_obj_align(compass_dial, LV_ALIGN_CENTER, 0, -4);
    lv_obj_set_style_radius(compass_dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(compass_dial, color_hex(0x0E1319), 0);
    lv_obj_set_style_bg_grad_color(compass_dial, color_hex(0x05080B), 0);
    lv_obj_set_style_bg_grad_dir(compass_dial, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(compass_dial, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(compass_dial, 2, 0);
    lv_obj_set_style_border_color(compass_dial, color_hex(0x31404C), 0);
    lv_obj_set_style_pad_all(compass_dial, 0, 0);
    lv_obj_set_style_shadow_width(compass_dial, 0, 0);

    compass_ring = lv_obj_create(compass_dial);
    lv_obj_clear_flag(compass_ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(compass_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(compass_ring, 142, 142);
    lv_obj_align(compass_ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(compass_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(compass_ring, color_hex(0x111922), 0);
    lv_obj_set_style_bg_opa(compass_ring, LV_OPA_30, 0);
    lv_obj_set_style_border_width(compass_ring, 1, 0);
    lv_obj_set_style_border_color(compass_ring, color_hex(0x2D3842), 0);
    lv_obj_set_style_shadow_width(compass_ring, 0, 0);

    create_compass_tick(compass_dial, 4, 18, 86, 10, LV_OPA_70);
    create_compass_tick(compass_dial, 18, 4, 148, 86, LV_OPA_70);
    create_compass_tick(compass_dial, 4, 18, 86, 148, LV_OPA_70);
    create_compass_tick(compass_dial, 18, 4, 10, 86, LV_OPA_70);
    create_compass_tick(compass_dial, 6, 6, 129, 29, LV_OPA_50);
    create_compass_tick(compass_dial, 6, 6, 129, 141, LV_OPA_50);
    create_compass_tick(compass_dial, 6, 6, 41, 29, LV_OPA_50);
    create_compass_tick(compass_dial, 6, 6, 41, 141, LV_OPA_50);

    compass_mark_n = lv_label_create(compass_dial);
    lv_label_set_text(compass_mark_n, "N");
    lv_obj_set_style_text_font(compass_mark_n, &lv_font_montserrat_16, 0);
    lv_obj_align(compass_mark_n, LV_ALIGN_TOP_MID, 0, 24);

    compass_mark_e = lv_label_create(compass_dial);
    lv_label_set_text(compass_mark_e, "E");
    lv_obj_set_style_text_font(compass_mark_e, &lv_font_montserrat_14, 0);
    lv_obj_align(compass_mark_e, LV_ALIGN_RIGHT_MID, -24, 0);

    compass_mark_s = lv_label_create(compass_dial);
    lv_label_set_text(compass_mark_s, "S");
    lv_obj_set_style_text_font(compass_mark_s, &lv_font_montserrat_14, 0);
    lv_obj_align(compass_mark_s, LV_ALIGN_BOTTOM_MID, 0, -22);

    compass_mark_w = lv_label_create(compass_dial);
    lv_label_set_text(compass_mark_w, "W");
    lv_obj_set_style_text_font(compass_mark_w, &lv_font_montserrat_14, 0);
    lv_obj_align(compass_mark_w, LV_ALIGN_LEFT_MID, 24, 0);

    compass_needle_north = lv_line_create(compass_dial);
    lv_obj_clear_flag(compass_needle_north, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(compass_needle_north, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(compass_needle_north, 176, 176);
    lv_obj_align(compass_needle_north, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_line_width(compass_needle_north, 5, 0);
    lv_obj_set_style_line_color(compass_needle_north, color_hex(0x64D8FF), 0);
    lv_obj_set_style_line_rounded(compass_needle_north, true, 0);
    compass_needle_north_points[0].x = 88;
    compass_needle_north_points[0].y = 88;
    compass_needle_north_points[1].x = 88;
    compass_needle_north_points[1].y = 28;
    lv_line_set_points(compass_needle_north, compass_needle_north_points, 2);

    compass_needle_south = lv_line_create(compass_dial);
    lv_obj_clear_flag(compass_needle_south, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(compass_needle_south, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(compass_needle_south, 176, 176);
    lv_obj_align(compass_needle_south, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_line_width(compass_needle_south, 3, 0);
    lv_obj_set_style_line_color(compass_needle_south, color_hex(0xC9D2DB), 0);
    lv_obj_set_style_line_rounded(compass_needle_south, true, 0);
    compass_needle_south_points[0].x = 88;
    compass_needle_south_points[0].y = 88;
    compass_needle_south_points[1].x = 88;
    compass_needle_south_points[1].y = 114;
    lv_line_set_points(compass_needle_south, compass_needle_south_points, 2);

    compass_center_dot = lv_obj_create(compass_dial);
    lv_obj_clear_flag(compass_center_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(compass_center_dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(compass_center_dot, 16, 16);
    lv_obj_align(compass_center_dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(compass_center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(compass_center_dot, color_hex(0x10161E), 0);
    lv_obj_set_style_bg_opa(compass_center_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(compass_center_dot, 2, 0);
    lv_obj_set_style_border_color(compass_center_dot, color_hex(0x64D8FF), 0);
    lv_obj_set_style_shadow_width(compass_center_dot, 0, 0);

    label_heading = lv_label_create(screen_compass);
    lv_label_set_text(label_heading, "000\xC2\xB0");
    lv_obj_set_style_text_font(label_heading, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_heading, get_accent_color(), 0);
    lv_obj_align(label_heading, LV_ALIGN_BOTTOM_RIGHT, -14, -12);
}

/* --- Build Pedometer Screen --- */
static void build_pedometer_screen(void)
{
    screen_pedometer = lv_obj_create(NULL);
    prepare_screen(screen_pedometer, 0x171008, 0x040506);
    add_backdrop_blob(screen_pedometer, 160, -55, -40, 0x35240F, LV_OPA_40);
    add_backdrop_blob(screen_pedometer, 145, 230, 145, 0x2A1620, LV_OPA_30);

    lv_obj_t *title = lv_label_create(screen_pedometer);
    lv_label_set_text(title, "PEDOMETER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    arc_steps = lv_arc_create(screen_pedometer);
    style_arc_widget(arc_steps, 188);
    lv_arc_set_range(arc_steps, 0, 10000);
    lv_arc_set_value(arc_steps, 0);
    lv_obj_align(arc_steps, LV_ALIGN_CENTER, 0, -10);

    pedometer_panel = create_panel(screen_pedometer, 160, 96);
    lv_obj_align(pedometer_panel, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *today = lv_label_create(pedometer_panel);
    lv_label_set_text(today, "TODAY");
    lv_obj_add_style(today, &style_title, 0);
    lv_obj_align(today, LV_ALIGN_TOP_MID, 0, 8);

    label_step_count = lv_label_create(pedometer_panel);
    lv_label_set_text(label_step_count, "0");
    lv_obj_set_style_text_font(label_step_count, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_step_count, color_hex(0xFFFFFF), 0);
    lv_obj_align(label_step_count, LV_ALIGN_CENTER, 0, -4);

    goal_pill = create_pill(screen_pedometer, 208, 28);
    lv_obj_align(goal_pill, LV_ALIGN_BOTTOM_MID, 0, -12);

    label_step_goal = lv_label_create(goal_pill);
    lv_label_set_text(label_step_goal, "0 / 10000 steps");
    lv_obj_set_style_text_font(label_step_goal, &lv_font_montserrat_14, 0);
    lv_obj_align(label_step_goal, LV_ALIGN_CENTER, 0, 0);
}

/* --- Build Status Popup --- */
static void build_status_modal(void)
{
    status_modal = lv_obj_create(lv_layer_top());
    lv_obj_clear_flag(status_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(status_modal, 236, 140);
    lv_obj_align(status_modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(status_modal, 20, 0);
    lv_obj_set_style_bg_color(status_modal, color_hex(0x12171E), 0);
    lv_obj_set_style_bg_grad_color(status_modal, color_hex(0x080A0E), 0);
    lv_obj_set_style_bg_grad_dir(status_modal, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(status_modal, 2, 0);
    lv_obj_set_style_shadow_width(status_modal, 24, 0);
    lv_obj_set_style_shadow_ofs_y(status_modal, 8, 0);
    lv_obj_set_style_shadow_opa(status_modal, LV_OPA_30, 0);
    lv_obj_add_flag(status_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status_modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_modal, touch_close_popup_cb, LV_EVENT_CLICKED, NULL);

    status_title = lv_label_create(status_modal);
    lv_label_set_text(status_title, "SYSTEM STATUS");
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_16, 0);
    lv_obj_align(status_title, LV_ALIGN_TOP_LEFT, 14, 12);

    status_label = lv_label_create(status_modal);
    lv_label_set_text(status_label, "BLE: Waiting...\nUptime: 0s\nTheme: Cyan");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, color_hex(0xF2F5F8), 0);
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 14, 12);

    lv_obj_t *hint = lv_label_create(status_modal);
    lv_label_set_text(hint, "tap anywhere to close");
    lv_obj_add_style(hint, &style_muted, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/* --- Build Brightness Overlay --- */
static void build_overlay(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t hor = disp ? lv_disp_get_hor_res(disp) : 320;
    lv_coord_t ver = disp ? lv_disp_get_ver_res(disp) : 240;

    overlay_layer = lv_obj_create(lv_layer_top());
    lv_obj_clear_flag(overlay_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(overlay_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(overlay_layer, hor, ver);
    lv_obj_set_style_bg_color(overlay_layer, color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay_layer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(overlay_layer, 0, 0);
    lv_obj_set_style_radius(overlay_layer, 0, 0);
    lv_obj_set_pos(overlay_layer, 0, 0);
}

/* --- Build Sleep Overlay --- */
static void build_sleep_overlay(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t hor = disp ? lv_disp_get_hor_res(disp) : 320;
    lv_coord_t ver = disp ? lv_disp_get_ver_res(disp) : 240;

    sleep_overlay = lv_obj_create(lv_layer_top());
    lv_obj_clear_flag(sleep_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sleep_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(sleep_overlay, hor, ver);
    lv_obj_set_style_bg_color(sleep_overlay, color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(sleep_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sleep_overlay, 0, 0);
    lv_obj_set_style_radius(sleep_overlay, 0, 0);
    lv_obj_set_pos(sleep_overlay, 0, 0);
    lv_obj_add_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* --- UI Init --- */
void ui_init(void)
{
    init_styles();
    build_sensor_screen();
    build_clock_screen();
    build_compass_screen();
    build_pedometer_screen();
    build_status_modal();
    build_overlay();
    build_sleep_overlay();

    apply_theme();

    lv_scr_load(screen_sensors);
    current_screen_idx = 0;
}

/* --- Button 1: Switch Screen (circular navigation) --- */
void ui_switch_screen(void)
{
    static lv_obj_t *screens[4];

    screens[0] = screen_sensors;
    screens[1] = screen_clock;
    screens[2] = screen_compass;
    screens[3] = screen_pedometer;

    current_screen_idx = (current_screen_idx + 1) % 4;
    lv_scr_load(screens[current_screen_idx]);
}

/* --- Button 2: Cycle Theme Color --- */
void ui_cycle_theme_color(void)
{
    theme_index = (theme_index + 1U) % THEME_COUNT;
    apply_theme();
}

/* --- Button 3: Toggle Brightness --- */
void ui_toggle_brightness(void)
{
    dark_overlay_enabled = !dark_overlay_enabled;
    lv_obj_set_style_bg_opa(overlay_layer,
                            dark_overlay_enabled ? 140 : LV_OPA_0, 0);
}

/* --- Button 4: Toggle Status Popup --- */
void ui_toggle_status_popup(void)
{
    if (lv_obj_has_flag(status_modal, LV_OBJ_FLAG_HIDDEN)) {
        snprintf(status_text, sizeof(status_text),
                 "BLE: %s\nUptime: %us\nTheme: %s",
                 ble_connected ? "Connected" : "Waiting...",
                 uptime_seconds,
                 get_theme_name());
        lv_label_set_text(status_label, status_text);
        lv_obj_clear_flag(status_modal, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(status_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_show_sleep_overlay(void)
{
    if (status_modal && !lv_obj_has_flag(status_modal, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(status_modal, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_move_foreground(sleep_overlay);
    lv_obj_clear_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
}

void ui_hide_sleep_overlay(void)
{
    lv_obj_add_flag(sleep_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* --- Data Updates --- */
void ui_update_temp_humidity(float temp, float hum)
{
    lv_label_set_text_fmt(label_temp_val, "%.1f C", (double)temp);
    lv_label_set_text_fmt(label_hum_val, "HUM  %.0f%%", (double)hum);
    lv_bar_set_value(bar_hum, (int32_t)hum, LV_ANIM_ON);
}

void ui_update_pressure(float press)
{
    lv_label_set_text_fmt(label_press_val, "PRESSURE  %.1f hPa",
                          (double)(press * 10.0f));
}

void ui_update_accel(float x, float y, float z)
{
    lv_label_set_text_fmt(label_accel_val, "ACC  %.1f  %.1f  %.1f",
                          (double)x, (double)y, (double)z);
}

void ui_update_gyro(float x, float y, float z)
{
    lv_label_set_text_fmt(label_gyro_val, "GYR  %.1f  %.1f  %.1f",
                          (double)x, (double)y, (double)z);
}

void ui_update_magnetometer(float x, float y, float z)
{
    lv_label_set_text_fmt(label_mag_val, "MAG  %.1f  %.1f  %.1f",
                          (double)x, (double)y, (double)z);
}

void ui_update_datetime(int year, int month, int day,
                        int hour, int minute, int second)
{
    snprintf(datetime_text, sizeof(datetime_text),
             "%02d/%02d/%04d  %02d:%02d:%02d",
             day, month, year, hour, minute, second);
    lv_label_set_text(label_datetime_small, datetime_text);

    lv_label_set_text_fmt(label_big_time, "%02d:%02d:%02d",
                          hour, minute, second);

    snprintf(date_text, sizeof(date_text), "%02d/%02d/%04d",
             day, month, year);
    lv_label_set_text(label_date, date_text);

    lv_arc_set_value(arc_seconds, second);
}

void ui_update_calendar(int year, int month, int day)
{
    if (calendar) {
        lv_calendar_set_today_date(calendar, year, month, day);
        lv_calendar_set_showed_date(calendar, year, month);
    }
}

void ui_set_ble_status(bool connected)
{
    ble_connected = connected;
}

void ui_update_uptime(uint32_t uptime_sec)
{
    uptime_seconds = uptime_sec;
}

void ui_update_compass(float heading_deg)
{
    float heading = fmodf(heading_deg, 360.0f);
    int h;
    float rad;
    float sin_a;
    float cos_a;

    if (heading < 0.0f) {
        heading += 360.0f;
    }

    h = (int)(heading + 0.5f);
    if (h >= 360) {
        h = 0;
    }

    rad = heading * 0.01745329252f;
    sin_a = sinf(rad);
    cos_a = cosf(rad);

    compass_needle_north_points[0].x = 88;
    compass_needle_north_points[0].y = 88;
    compass_needle_north_points[1].x = 88 + (lv_coord_t)(sin_a * 60.0f);
    compass_needle_north_points[1].y = 88 - (lv_coord_t)(cos_a * 60.0f);

    compass_needle_south_points[0].x = 88;
    compass_needle_south_points[0].y = 88;
    compass_needle_south_points[1].x = 88 - (lv_coord_t)(sin_a * 26.0f);
    compass_needle_south_points[1].y = 88 + (lv_coord_t)(cos_a * 26.0f);

    if (compass_needle_north) {
        lv_line_set_points(compass_needle_north, compass_needle_north_points, 2);
    }
    if (compass_needle_south) {
        lv_line_set_points(compass_needle_south, compass_needle_south_points, 2);
    }

    lv_label_set_text_fmt(label_heading, "%03d\xC2\xB0", h);
}

void ui_update_steps(uint32_t step_count, uint32_t goal)
{
    if (goal == 0U) {
        goal = 10000U;
    }

    lv_arc_set_range(arc_steps, 0, (int32_t)goal);
    lv_arc_set_value(arc_steps,
                     (int32_t)(step_count > goal ? goal : step_count));
    lv_label_set_text_fmt(label_step_count, "%u", step_count);
    lv_label_set_text_fmt(label_step_goal, "%u / %u steps", step_count, goal);
}
