/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/keycode_state_changed.h>

#include <zmk/wpm.h>

#define WPM_INTERVAL_MS CONFIG_ZMK_WPM_UPDATE_INTERVAL_MS
#define WPM_INTERVALS (CONFIG_ZMK_WPM_WINDOW_MS / CONFIG_ZMK_WPM_UPDATE_INTERVAL_MS)

BUILD_ASSERT(CONFIG_ZMK_WPM_WINDOW_MS % CONFIG_ZMK_WPM_UPDATE_INTERVAL_MS == 0,
             "CONFIG_ZMK_WPM_WINDOW_MS must be a multiple of CONFIG_ZMK_WPM_UPDATE_INTERVAL_MS");

// Keys after a pause are averaged over at least this long, so a single quick key isn't a burst.
#define WPM_MIN_ELAPSED_MS 1000

// See https://en.wikipedia.org/wiki/Words_per_minute
// "Since the length or duration of words is clearly variable, for the purpose of measurement of
// text entry, the definition of each "word" is often standardized to be five characters or
// keystrokes long in English"
#define CHARS_PER_WORD 5.0

static uint8_t wpm_state;

// Key releases in each interval of the window, the current interval's at current_interval.
static uint16_t interval_counts[WPM_INTERVALS];
static uint16_t current_interval;
static uint32_t window_count;
// Intervals since typing resumed, up to the window's length.
static uint16_t intervals_elapsed;
static struct k_spinlock lock;

int zmk_wpm_get_state(void) { return wpm_state; }

static void wpm_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(wpm_work, wpm_work_handler);

int wpm_event_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    // count only key up events
    if (ev && !ev->state) {
        K_SPINLOCK(&lock) {
            interval_counts[current_interval]++;
            window_count++;
        }
        LOG_DBG("window_count %d keycode %d", window_count, ev->keycode);
        // Starts the updates after a pause; while they run, this does nothing.
        k_work_schedule(&wpm_work, K_MSEC(WPM_INTERVAL_MS));
    }
    return 0;
}

static void wpm_work_handler(struct k_work *work) {
    bool idle = false;
    uint8_t new_state;

    K_SPINLOCK(&lock) {
        intervals_elapsed = MIN(intervals_elapsed + 1, WPM_INTERVALS);
        uint32_t elapsed_ms = MAX(intervals_elapsed * WPM_INTERVAL_MS, WPM_MIN_ELAPSED_MS);
        new_state = MIN((window_count / CHARS_PER_WORD) / (elapsed_ms / 60000.0), UINT8_MAX);

        // The oldest interval leaves the window.
        current_interval = (current_interval + 1) % WPM_INTERVALS;
        window_count -= interval_counts[current_interval];
        interval_counts[current_interval] = 0;

        if (window_count == 0 && new_state == 0) {
            intervals_elapsed = 0;
            idle = true;
        }
    }

    if (new_state != wpm_state) {
        wpm_state = new_state;
        LOG_DBG("Raised WPM state changed %d", wpm_state);
        raise_zmk_wpm_state_changed((struct zmk_wpm_state_changed){.state = wpm_state});
    }

    // Idle: no updates until the next key.
    if (!idle) {
        k_work_schedule(&wpm_work, K_MSEC(WPM_INTERVAL_MS));
    }
}

ZMK_LISTENER(wpm, wpm_event_listener);
ZMK_SUBSCRIPTION(wpm, zmk_keycode_state_changed);
