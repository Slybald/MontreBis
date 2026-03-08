/*
 * Previous UI implementation snapshot.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui.h"
#include <lvgl.h>
#include <stdio.h>

/* Convert RGB colors to the display format. */
static inline uint32_t fix_color(uint32_t color) {
    return ((color & 0xFF0000) >> 16) | (color & 0x00FF00) | ((color & 0x0000FF) << 16);
}

/* Theme and styles */
static lv_style_t style_card;
static lv_style_t style_title;
static uint32_t current_accent_color = 0x00FFFF; /* Cyan default */
static uint8_t theme_index = 0;
static bool dark_overlay_enabled = false;
static bool ble_connected = false;
static uint32_t uptime_seconds = 0;

/* Screens */
static lv_obj_t *screen_sensors;
static lv_obj_t *screen_clock;
static lv_obj_t *screen_compass;
static lv_obj_t *screen_pedometer;
static lv_obj_t *overlay_layer;
static lv_obj_t *status_modal;
static lv_obj_t *status_label;

/* Circular navigation order: sensors -> clock -> compass -> pedometer */
static int current_screen_idx = 0;

/* Sensor screen widgets */
static lv_obj_t *label_temp_val;
static lv_obj_t *label_hum_val;
static lv_obj_t *bar_hum;
static lv_obj_t *label_press_val;
static lv_obj_t *label_accel_val;
static lv_obj_t *label_gyro_val;
static lv_obj_t *label_mag_val;
static lv_obj_t *label_datetime_small;

/* Clock screen widgets */
static lv_obj_t *arc_seconds;
static lv_obj_t *label_big_time;
static lv_obj_t *label_date;
static lv_obj_t *calendar;

/* Compass screen widgets */
static lv_obj_t *arc_compass;
static lv_obj_t *label_heading;
static lv_obj_t *label_cardinal;

/* Pedometer screen widgets */
static lv_obj_t *arc_steps;
static lv_obj_t *label_step_count;
static lv_obj_t *label_step_goal;

/* Text buffers */
static char datetime_text[32];
static char date_text[16];
static char status_text[128];

/* Return the current accent color. */
static lv_color_t get_accent_color(void) {
    return lv_color_hex(fix_color(current_accent_color));
}

/* Touch callbacks */
static void touch_switch_screen_cb(lv_event_t *e) {
    (void)e;
    ui_switch_screen();
}

static void touch_toggle_calendar_cb(lv_event_t *e) {
    (void)e;
    if (lv_obj_has_flag(calendar, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(calendar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(calendar, LV_OBJ_FLAG_HIDDEN);
    }
}

static void touch_close_popup_cb(lv_event_t *e) {
    (void)e;
    if (!lv_obj_has_flag(status_modal, LV_OBJ_FLAG_HIDDEN)) {
        ui_toggle_status_popup();
    }
}

/* Initialize styles. */
static void init_styles(void) {
    /* Card style: dark gray background with rounded corners */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_hex(fix_color(0x202020)));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_pad_all(&style_card, 8);
    lv_style_set_border_width(&style_card, 0);

    /* Title style: muted gray text */
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_title, lv_color_hex(fix_color(0x888888)));
}

/* Create a data card. */
static lv_obj_t* create_card(lv_obj_t *parent, const char *title, int w, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_size(card, w, h);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_add_style(lbl, &style_title, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, -2);
    
    return card;
}

/* Build the sensor screen. */
static void build_sensor_screen(void) {
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t hor = disp ? lv_disp_get_hor_res(disp) : 320;
    lv_coord_t ver = disp ? lv_disp_get_ver_res(disp) : 240;
    LV_UNUSED(ver);

    screen_sensors = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_sensors, lv_color_hex(fix_color(0x000000)), 0);

    /* Header */
    lv_obj_t *header = lv_label_create(screen_sensors);
    lv_label_set_text(header, "CONNECTED WATCH");
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    /* Environment card */
    lv_coord_t card_width = hor - 10;
    if (card_width < 180) card_width = 180;

    lv_obj_t *card_env = create_card(screen_sensors, "ENVIRONMENT", card_width, 70);
    lv_obj_align(card_env, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_add_flag(card_env, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card_env, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    /* Temperature */
    label_temp_val = lv_label_create(card_env);
    lv_label_set_text(label_temp_val, "--.- C");
    lv_obj_set_style_text_font(label_temp_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_temp_val, get_accent_color(), 0);
    lv_obj_align(label_temp_val, LV_ALIGN_LEFT_MID, 0, 8);

    /* Humidity % label */
    label_hum_val = lv_label_create(card_env);
    lv_label_set_text(label_hum_val, "--%");
    lv_obj_set_style_text_font(label_hum_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_hum_val, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(label_hum_val, LV_ALIGN_TOP_RIGHT, -5, 12);

    /* Humidity bar */
    bar_hum = lv_bar_create(card_env);
    lv_obj_set_size(bar_hum, 80, 6);
    lv_obj_align(bar_hum, LV_ALIGN_RIGHT_MID, -5, 12);
    lv_bar_set_range(bar_hum, 0, 100);
    lv_bar_set_value(bar_hum, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_hum, lv_color_hex(fix_color(0x404040)), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_hum, get_accent_color(), LV_PART_INDICATOR);

    /* Pressure */
    label_press_val = lv_label_create(card_env);
    lv_label_set_text(label_press_val, "---- hPa");
    lv_obj_set_style_text_font(label_press_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_press_val, lv_color_hex(fix_color(0xCCCCCC)), 0);
    lv_obj_align(label_press_val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Motion card */
    lv_obj_t *card_motion = create_card(screen_sensors, "MOTION", card_width, 75);
    lv_obj_align(card_motion, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_add_flag(card_motion, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card_motion, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    label_accel_val = lv_label_create(card_motion);
    lv_label_set_text(label_accel_val, "Acc: -- -- --");
    lv_obj_set_style_text_font(label_accel_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_accel_val, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(label_accel_val, LV_ALIGN_TOP_LEFT, 0, 12);

    label_gyro_val = lv_label_create(card_motion);
    lv_label_set_text(label_gyro_val, "Gyr: -- -- --");
    lv_obj_set_style_text_font(label_gyro_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_gyro_val, lv_color_hex(fix_color(0xAAAAAA)), 0);
    lv_obj_align(label_gyro_val, LV_ALIGN_LEFT_MID, 0, 8);

    label_mag_val = lv_label_create(card_motion);
    lv_label_set_text(label_mag_val, "Mag: -- -- --");
    lv_obj_set_style_text_font(label_mag_val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_mag_val, lv_color_hex(fix_color(0x888888)), 0);
    lv_obj_align(label_mag_val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* Date and time */
    label_datetime_small = lv_label_create(screen_sensors);
    lv_label_set_text(label_datetime_small, "--/--/---- --:--:--");
    lv_obj_set_style_text_font(label_datetime_small, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_datetime_small, get_accent_color(), 0);
    lv_obj_align(label_datetime_small, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_flag(label_datetime_small, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_datetime_small, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);
}

/* Build the clock screen. */
static void build_clock_screen(void) {
    screen_clock = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_clock, lv_color_hex(fix_color(0x000000)), 0);

    /* Seconds arc */
    arc_seconds = lv_arc_create(screen_clock);
    lv_obj_set_size(arc_seconds, 200, 200);
    lv_arc_set_rotation(arc_seconds, 270);
    lv_arc_set_bg_angles(arc_seconds, 0, 360);
    lv_arc_set_range(arc_seconds, 0, 60);
    lv_arc_set_value(arc_seconds, 0);
    lv_obj_remove_style(arc_seconds, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_seconds, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc_seconds, lv_color_hex(fix_color(0x303030)), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_seconds, get_accent_color(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_seconds, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_seconds, 8, LV_PART_INDICATOR);
    lv_obj_align(arc_seconds, LV_ALIGN_CENTER, 0, -10);

    /* Time label */
    label_big_time = lv_label_create(screen_clock);
    lv_label_set_text(label_big_time, "--:--:--");
    lv_obj_set_style_text_font(label_big_time, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_big_time, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(label_big_time, LV_ALIGN_CENTER, 0, -25);
    lv_obj_add_flag(label_big_time, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_big_time, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    /* Date label */
    label_date = lv_label_create(screen_clock);
    lv_label_set_text(label_date, "--/--/----");
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_date, get_accent_color(), 0);
    lv_obj_align(label_date, LV_ALIGN_CENTER, 0, 5);
    lv_obj_add_flag(label_date, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label_date, touch_toggle_calendar_cb, LV_EVENT_CLICKED, NULL);

    /* Calendar */
    calendar = lv_calendar_create(screen_clock);
    lv_obj_set_size(calendar, 180, 140);
    lv_obj_align(calendar, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_calendar_set_today_date(calendar, 1970, 1, 1);
    lv_calendar_set_showed_date(calendar, 1970, 1);
    lv_obj_add_flag(calendar, LV_OBJ_FLAG_HIDDEN);
}

/* Build the compass screen. */
static void build_compass_screen(void) {
    screen_compass = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_compass, lv_color_hex(fix_color(0x000000)), 0);

    /* Title */
    lv_obj_t *title = lv_label_create(screen_compass);
    lv_label_set_text(title, "COMPASS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    /* Heading arc */
    arc_compass = lv_arc_create(screen_compass);
    lv_obj_set_size(arc_compass, 200, 200);
    lv_arc_set_rotation(arc_compass, 270);
    lv_arc_set_bg_angles(arc_compass, 0, 360);
    lv_arc_set_range(arc_compass, 0, 360);
    lv_arc_set_value(arc_compass, 0);
    lv_obj_remove_style(arc_compass, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_compass, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc_compass, lv_color_hex(fix_color(0x303030)), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_compass, get_accent_color(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_compass, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_compass, 8, LV_PART_INDICATOR);
    lv_obj_align(arc_compass, LV_ALIGN_CENTER, 0, -10);

    /* Heading in degrees */
    label_heading = lv_label_create(screen_compass);
    lv_label_set_text(label_heading, "0\xC2\xB0");
    lv_obj_set_style_text_font(label_heading, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_heading, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(label_heading, LV_ALIGN_CENTER, 0, -20);

    /* Cardinal direction */
    label_cardinal = lv_label_create(screen_compass);
    lv_label_set_text(label_cardinal, "N");
    lv_obj_set_style_text_font(label_cardinal, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label_cardinal, get_accent_color(), 0);
    lv_obj_align(label_cardinal, LV_ALIGN_CENTER, 0, 15);
}

/* Build the pedometer screen. */
static void build_pedometer_screen(void) {
    screen_pedometer = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_pedometer, lv_color_hex(fix_color(0x000000)), 0);

    /* Title */
    lv_obj_t *title = lv_label_create(screen_pedometer);
    lv_label_set_text(title, "PEDOMETER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title, touch_switch_screen_cb, LV_EVENT_CLICKED, NULL);

    /* Steps arc */
    arc_steps = lv_arc_create(screen_pedometer);
    lv_obj_set_size(arc_steps, 200, 200);
    lv_arc_set_rotation(arc_steps, 270);
    lv_arc_set_bg_angles(arc_steps, 0, 360);
    lv_arc_set_range(arc_steps, 0, 10000);
    lv_arc_set_value(arc_steps, 0);
    lv_obj_remove_style(arc_steps, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_steps, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc_steps, lv_color_hex(fix_color(0x303030)), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_steps, get_accent_color(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_steps, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_steps, 8, LV_PART_INDICATOR);
    lv_obj_align(arc_steps, LV_ALIGN_CENTER, 0, -10);

    /* Step count */
    label_step_count = lv_label_create(screen_pedometer);
    lv_label_set_text(label_step_count, "0");
    lv_obj_set_style_text_font(label_step_count, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_step_count, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(label_step_count, LV_ALIGN_CENTER, 0, -20);

    /* Goal */
    label_step_goal = lv_label_create(screen_pedometer);
    lv_label_set_text(label_step_goal, "0 / 10000 steps");
    lv_obj_set_style_text_font(label_step_goal, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_step_goal, get_accent_color(), 0);
    lv_obj_align(label_step_goal, LV_ALIGN_CENTER, 0, 15);
}

/* Build the status popup. */
static void build_status_modal(void) {
    status_modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(status_modal, 220, 130);
    lv_obj_align(status_modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(status_modal, lv_color_hex(fix_color(0x202020)), 0);
    lv_obj_set_style_border_color(status_modal, get_accent_color(), 0);
    lv_obj_set_style_border_width(status_modal, 2, 0);
    lv_obj_set_style_radius(status_modal, 10, 0);
    lv_obj_add_flag(status_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(status_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(status_modal, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_modal, touch_close_popup_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *title = lv_label_create(status_modal);
    lv_label_set_text(title, "SYSTEM STATUS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, get_accent_color(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    status_label = lv_label_create(status_modal);
    lv_label_set_text(status_label, "BLE: Waiting...\nUptime: 0s\nTheme: Cyan");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(fix_color(0xFFFFFF)), 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 15);
}

/* Build the dim overlay. */
static void build_overlay(void) {
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t hor = disp ? lv_disp_get_hor_res(disp) : 320;
    lv_coord_t ver = disp ? lv_disp_get_ver_res(disp) : 240;

    overlay_layer = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay_layer, hor, ver);
    lv_obj_set_style_bg_color(overlay_layer, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay_layer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(overlay_layer, 0, 0);
    lv_obj_clear_flag(overlay_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(overlay_layer, 0, 0);
}

/* Initialize the UI. */
void ui_init(void) {
    init_styles();
    build_sensor_screen();
    build_clock_screen();
    build_compass_screen();
    build_pedometer_screen();
    build_status_modal();
    build_overlay();

    lv_scr_load(screen_sensors);
    current_screen_idx = 0;
}

/* Switch to the next screen. */
void ui_switch_screen(void) {
    static lv_obj_t *screens[4];
    screens[0] = screen_sensors;
    screens[1] = screen_clock;
    screens[2] = screen_compass;
    screens[3] = screen_pedometer;

    current_screen_idx = (current_screen_idx + 1) % 4;
    lv_scr_load_anim(screens[current_screen_idx], LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

/* Cycle the accent theme. */
void ui_cycle_theme_color(void) {
    theme_index = (theme_index + 1) % 4;
    switch (theme_index) {
        case 0: current_accent_color = 0x00FFFF; break; /* Cyan */
        case 1: current_accent_color = 0x00FF00; break; /* Green */
        case 2: current_accent_color = 0xFFA500; break; /* Orange */
        case 3: current_accent_color = 0xFF00FF; break; /* Magenta */
    }

    lv_color_t c = get_accent_color();

    /* Update accent-colored elements */
    lv_obj_set_style_text_color(label_temp_val, c, 0);
    lv_obj_set_style_bg_color(bar_hum, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(label_datetime_small, c, 0);
    lv_obj_set_style_arc_color(arc_seconds, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(label_date, c, 0);
    lv_obj_set_style_border_color(status_modal, c, 0);
    lv_obj_set_style_arc_color(arc_compass, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(label_cardinal, c, 0);
    lv_obj_set_style_arc_color(arc_steps, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(label_step_goal, c, 0);
}

/* Toggle the dim overlay. */
void ui_toggle_brightness(void) {
    dark_overlay_enabled = !dark_overlay_enabled;
    lv_obj_set_style_bg_opa(overlay_layer, dark_overlay_enabled ? 150 : LV_OPA_0, 0);
}

/* Toggle the status popup. */
void ui_toggle_status_popup(void) {
    if (lv_obj_has_flag(status_modal, LV_OBJ_FLAG_HIDDEN)) {
        /* Update the status text before showing the popup */
        const char *theme_names[] = {"Cyan", "Green", "Orange", "Magenta"};
        snprintf(status_text, sizeof(status_text),
                 "BLE: %s\nUptime: %us\nTheme: %s",
                 ble_connected ? "Connected" : "Waiting...",
                 uptime_seconds,
                 theme_names[theme_index]);
        lv_label_set_text(status_label, status_text);
        lv_obj_clear_flag(status_modal, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(status_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Data updates */

void ui_update_temp_humidity(float temp, float hum) {
    lv_label_set_text_fmt(label_temp_val, "%.1f C", temp);
    lv_label_set_text_fmt(label_hum_val, "%.0f%%", hum);
    lv_bar_set_value(bar_hum, (int32_t)hum, LV_ANIM_ON);
}

void ui_update_pressure(float press) {
    lv_label_set_text_fmt(label_press_val, "%.1f hPa", (double)(press * 10.0f));
}

void ui_update_accel(float x, float y, float z) {
    lv_label_set_text_fmt(label_accel_val, "Acc: %.1f %.1f %.1f", x, y, z);
}

void ui_update_gyro(float x, float y, float z) {
    lv_label_set_text_fmt(label_gyro_val, "Gyr: %.1f %.1f %.1f", x, y, z);
}

void ui_update_magnetometer(float x, float y, float z) {
    lv_label_set_text_fmt(label_mag_val, "Mag: %.1f %.1f %.1f", x, y, z);
}

void ui_update_datetime(int year, int month, int day, int hour, int minute, int second) {
    /* Sensor screen datetime */
    snprintf(datetime_text, sizeof(datetime_text), "%02d/%02d/%04d %02d:%02d:%02d",
             day, month, year, hour, minute, second);
    lv_label_set_text(label_datetime_small, datetime_text);

    /* Clock screen: time with seconds (big) */
    lv_label_set_text_fmt(label_big_time, "%02d:%02d:%02d", hour, minute, second);

    /* Clock screen: date */
    snprintf(date_text, sizeof(date_text), "%02d/%02d/%04d", day, month, year);
    lv_label_set_text(label_date, date_text);

    /* Update arc with seconds (0-60) */
    lv_arc_set_value(arc_seconds, second);
}

void ui_update_calendar(int year, int month, int day) {
    if (calendar) {
        lv_calendar_set_today_date(calendar, year, month, day);
        lv_calendar_set_showed_date(calendar, year, month);
    }
}

void ui_set_ble_status(bool connected) {
    ble_connected = connected;
}

void ui_update_uptime(uint32_t uptime_sec) {
    uptime_seconds = uptime_sec;
}

void ui_update_compass(float heading_deg) {
    lv_arc_set_value(arc_compass, (int32_t)heading_deg);
    lv_label_set_text_fmt(label_heading, "%d\xC2\xB0", (int)heading_deg);

    const char *dir;
    int h = (int)heading_deg;
    if (h >= 337 || h < 23)  dir = "N";
    else if (h < 68)         dir = "NE";
    else if (h < 113)        dir = "E";
    else if (h < 158)        dir = "SE";
    else if (h < 203)        dir = "S";
    else if (h < 248)        dir = "SW";
    else if (h < 293)        dir = "W";
    else                     dir = "NW";

    lv_label_set_text(label_cardinal, dir);
}

void ui_update_steps(uint32_t step_count, uint32_t goal) {
    if (goal == 0) goal = 10000;
    lv_arc_set_range(arc_steps, 0, (int32_t)goal);
    lv_arc_set_value(arc_steps, (int32_t)(step_count > goal ? goal : step_count));
    lv_label_set_text_fmt(label_step_count, "%u", step_count);
    lv_label_set_text_fmt(label_step_goal, "%u / %u steps", step_count, goal);
}
