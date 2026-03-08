/* NVS Settings Persistence — SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "storage.h"

LOG_MODULE_REGISTER(storage, CONFIG_LOG_DEFAULT_LEVEL);

static uint8_t  stored_theme;
static uint32_t stored_steps;
static uint32_t stored_last_sync;

static int storage_set(const char *name, size_t len,
                       settings_read_cb read_cb, void *cb_arg)
{
    if (!strcmp(name, "theme")) {
        if (len != sizeof(stored_theme)) return -EINVAL;
        return read_cb(cb_arg, &stored_theme, sizeof(stored_theme));
    }
    if (!strcmp(name, "steps")) {
        if (len != sizeof(stored_steps)) return -EINVAL;
        return read_cb(cb_arg, &stored_steps, sizeof(stored_steps));
    }
    if (!strcmp(name, "sync")) {
        if (len != sizeof(stored_last_sync)) return -EINVAL;
        return read_cb(cb_arg, &stored_last_sync, sizeof(stored_last_sync));
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(watch, "watch", NULL, storage_set, NULL, NULL);

void storage_init(void)
{
    int err = settings_subsys_init();
    if (err) {
        LOG_ERR("Settings init failed: %d", err);
        return;
    }
    settings_load();
    LOG_INF("Settings loaded (theme=%u, steps=%u, sync=%u)",
            stored_theme, stored_steps, stored_last_sync);
}

void storage_save_theme(uint8_t theme_idx)
{
    stored_theme = theme_idx;
    settings_save_one("watch/theme", &stored_theme, sizeof(stored_theme));
}

uint8_t storage_load_theme(void)
{
    return stored_theme;
}

void storage_save_steps(uint32_t steps)
{
    stored_steps = steps;
    settings_save_one("watch/steps", &stored_steps, sizeof(stored_steps));
}

uint32_t storage_load_steps(void)
{
    return stored_steps;
}

void storage_save_last_sync(uint32_t timestamp)
{
    stored_last_sync = timestamp;
    settings_save_one("watch/sync", &stored_last_sync, sizeof(stored_last_sync));
}

uint32_t storage_load_last_sync(void)
{
    return stored_last_sync;
}
