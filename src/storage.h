/* NVS Settings Persistence — SPDX-License-Identifier: Apache-2.0 */
#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

void storage_init(void);

void storage_save_theme(uint8_t theme_idx);
uint8_t storage_load_theme(void);

void storage_save_steps(uint32_t steps);
uint32_t storage_load_steps(void);

void storage_save_last_sync(uint32_t timestamp);
uint32_t storage_load_last_sync(void);

#endif
