/*
 * Shared event types and kernel objects.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/atomic.h>
#include <stdbool.h>

/* Event types posted on the main queue. */
enum event_type {
    EVT_NONE = 0,

    /* Button events */
    EVT_BTN_SWITCH_SCREEN,    /* SW0 : basculer écran Capteurs ↔ Horloge    */
    EVT_BTN_CYCLE_THEME,      /* SW1 : cycler la couleur d'accent            */
    EVT_BTN_BRIGHTNESS,       /* SW2 : basculer overlay luminosité           */
    EVT_BTN_STATUS_POPUP,     /* SW3 : afficher / masquer popup status       */

    /* Timer events */
    EVT_TICK_SENSORS,         /* 250 ms : déclencher lecture capteurs I2C    */
    EVT_TICK_TIME,            /* 1 s : vérifier timeout inactivité           */

    /* BLE events */
    EVT_BLE_TIME_SYNC,        /* Timestamp Unix reçu du smartphone           */

    /* Internal events */
    EVT_SENSOR_DATA_READY,    /* Lecture capteurs terminée                    */
};

/* Message format stored in the event queue. */
struct event_msg {
    enum event_type type;
    union {
        uint32_t timestamp;   /* Payload pour EVT_BLE_TIME_SYNC              */
        uint8_t  value;       /* Payload générique                           */
    } data;
};

/* Sensor values shared between the controller and the display thread. */
#define SNAP_VALID_ENV    BIT(0)
#define SNAP_VALID_PRESS  BIT(1)
#define SNAP_VALID_MOTION BIT(2)
#define SNAP_VALID_MAG    BIT(3)

struct sensor_snapshot {
    float   temperature;      /* °C  (HTS221)                                */
    float   humidity;         /* %   (HTS221)                                */
    float   pressure;         /* kPa (LPS22HH)                               */
    float   accel[3];         /* m/s² x,y,z (LSM6DSO)                        */
    float   gyro[3];          /* dps  x,y,z (LSM6DSO)                        */
    float   magn[3];          /* gauss x,y,z (LIS2MDL)                       */
    uint8_t valid_flags;      /* bitmask: SNAP_VALID_*                        */
};

/* Raw inputs posted by interrupts and timers. */
enum raw_input {
    RAW_BTN0,
    RAW_BTN1,
    RAW_BTN2,
    RAW_BTN3,
    RAW_TIMER_SENSORS,
    RAW_TIMER_TIME,
};

/* Atomic UI actions posted by the controller. */
#define UI_ACT_SWITCH_SCREEN   BIT(0)
#define UI_ACT_CYCLE_THEME     BIT(1)
#define UI_ACT_BRIGHTNESS      BIT(2)
#define UI_ACT_STATUS_POPUP    BIT(3)
#define UI_ACT_ROTATION_CHANGE BIT(4)
#define UI_ACT_ENTER_SLEEP     BIT(5)
#define UI_ACT_EXIT_SLEEP      BIT(6)

/* Controller state machine. */
enum app_state {
    STATE_INIT,    /* Démarrage système, attente premier événement       */
    STATE_IDLE,    /* Fonctionnement normal, attente d'événements        */
    STATE_ACTIVE,  /* Traitement en cours (lecture capteurs, sync BLE)   */
    STATE_SLEEP,   /* Écran éteint, capteurs en pause, veille            */
};

/* Shared kernel objects. */
extern struct k_msgq event_bus;        /* Bus principal d'événements     */
extern struct k_msgq raw_input_q;      /* File ISR/Timer → INPUT_MGR     */
extern struct k_sem  sem_sensor_start; /* CONTROLLER → SENSORS           */
extern struct k_sem  sem_display_ready;/* main() → DISPLAY               */
extern struct k_sem  sem_display_wake; /* CONTROLLER → DISPLAY wake      */
extern struct k_mutex mtx_sensors;     /* Protection shared_sensors      */

extern struct sensor_snapshot shared_sensors;

extern atomic_t ui_action_flags;
extern atomic_t sensor_display_dirty;
extern atomic_t display_sleeping;
extern atomic_t shared_step_count;
extern atomic_t shared_heading_centideg;
extern atomic_t touch_activity_flag;

/* Small queue helpers. */

/** Poster un événement simple (sans données) sur l'event bus.
 *  Returns -EBUSY if queue full. Caller in thread context should log on failure.
 */
static inline int event_post(enum event_type type)
{
    struct event_msg msg = { .type = type };
    return k_msgq_put(&event_bus, &msg, K_NO_WAIT);
}

/** Poster un événement avec payload uint32 sur l'event bus.
 *  Returns -EBUSY if queue full. Caller in thread context should log on failure.
 */
static inline int event_post_val(enum event_type type, uint32_t val)
{
    struct event_msg msg = { .type = type, .data = { .timestamp = val } };
    return k_msgq_put(&event_bus, &msg, K_NO_WAIT);
}

/** Poster un input brut (ISR-safe, K_NO_WAIT).
 *  Perte possible si file pleine (pas de log depuis ISR).
 */
static inline int raw_input_post(enum raw_input input)
{
    return k_msgq_put(&raw_input_q, &input, K_NO_WAIT);
}

#endif /* APP_EVENTS_H */
