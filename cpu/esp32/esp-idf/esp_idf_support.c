/*
 * Copyright (C) 2022 Gunar Schorcht
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_esp32
 * @{
 *
 * @file
 * @brief       Functions required for ESP-IDF compatibility
 *
 * @author      Gunar Schorcht <gunar@schorcht.net>
 *
 * @}
 */

#include <string.h>

#include "esp_common.h"
#include "log.h"
#include "syscalls.h"

#include "esp_attr.h"
#include "esp_log.h"

#define ENABLE_DEBUG  0
#include "debug.h"

#if IS_USED(MODULE_ESP_WIFI_ANY) || IS_USED(MODULE_ESP_ETH)
#include "esp_event_base.h"

ESP_EVENT_DEFINE_BASE(IP_EVENT);
#endif

/*
 * provided by: /path/to/esp-idf/components/log/log_freertos.c
 */
uint32_t IRAM_ATTR esp_log_timestamp(void)
{
    return system_get_time() / USEC_PER_MSEC;
}

typedef struct {
    const char *tag;
    unsigned level;
} esp_log_level_entry_t;

static esp_log_level_entry_t _log_levels[] = {
    { .tag = "wifi", .level = LOG_INFO },
    { .tag = "*", .level = LOG_DEBUG },
};

/*
 * provided by: /path/to/esp-idf/components/log/log.c
 */
void IRAM_ATTR esp_log_write(esp_log_level_t level,
                             const char* tag, const char* format, ...)
{
    va_list list;
    va_start(list, format);
    esp_log_writev(level, tag, format, list);
    va_end(list);
}

/*
 * provided by: /path/to/esp-idf/components/log/log.c
 */
void IRAM_ATTR esp_log_writev(esp_log_level_t level,
                              const char *tag,
                              const char *format,
                              va_list args)
{
    /*
     * We use the log level set for the given tag instead of using
     * the given log level.
     */
    esp_log_level_t act_level = (esp_log_level_t)LOG_DEBUG;
    size_t i;
    for (i = 0; i < ARRAY_SIZE(_log_levels); i++) {
        if (strcmp(tag, _log_levels[i].tag) == 0) {
            act_level = _log_levels[i].level;
            break;
        }
    }

    /* If we didn't find an entry for the tag, we use the log level for "*" */
    if (i == ARRAY_SIZE(_log_levels)) {
        act_level = _log_levels[ARRAY_SIZE(_log_levels)-1].level;
    }

    /* Return if the log output has not the required level */
    if ((unsigned)act_level > CONFIG_LOG_DEFAULT_LEVEL) {
        return;
    }

    vprintf(format, args);
}

/*
 * provided by: /path/to/esp-idf/components/log/log.c
 */
void esp_log_level_set(const char* tag, esp_log_level_t level)
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(_log_levels); i++) {
        if (strcmp(tag, _log_levels[i].tag) == 0) {
            break;
        }
    }

    if (i == ARRAY_SIZE(_log_levels)) {
        LOG_DEBUG("Tag for setting log level not found\n");
        return;
    }

    _log_levels[i].level = level;
}

/*
 * provided by: /path/to/esp-idf/components/newlib/time.c
 */
void esp_newlib_time_init(void)
{
    extern void esp_time_impl_init(void);
    esp_time_impl_init();
}
