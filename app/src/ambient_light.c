/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * app/src/ambient_light.c
 *
 * Ambient light sensor service for ZMK.
 *
 * Polls the sensor aliased as "zmk-als" in devicetree at a configurable
 * interval, stores the latest lux reading, and notifies registered
 * callbacks.  On the dongle-screen shield for seeeduino_xiao_ble, the
 * APDS9960 is already declared in the board overlay and only needs
 * CONFIG_ZMK_AMBIENT_LIGHT=y plus the alias added to the overlay.
 *
 * API:
 *   zmk_ambient_light_get_lux()          - read latest lux value
 *   zmk_ambient_light_lux_to_brt()       - map lux → brightness percent
 *   zmk_ambient_light_register_cb()      - push notifications
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/ambient_light.h>

LOG_MODULE_REGISTER(zmk_ambient_light, CONFIG_ZMK_AMBIENT_LIGHT_LOG_LEVEL);

#define ZMK_ALS_NODE DT_ALIAS(zmk_als)

#if !DT_NODE_EXISTS(ZMK_ALS_NODE)
#error "No 'zmk-als' alias found. Add one to your board overlay pointing to the APDS9960 node."
#endif

static const struct device *const als_dev = DEVICE_DT_GET(ZMK_ALS_NODE);

static uint32_t current_lux;
static zmk_ambient_light_cb_t callbacks[CONFIG_ZMK_AMBIENT_LIGHT_MAX_CALLBACKS];
static uint8_t cb_count;
static struct k_work_delayable als_work;

/* ------------------------------------------------------------------ */
/* Lux → brightness mapping                                           */
/*                                                                    */
/* Uses a fixed-point log2 with fractional interpolation within each  */
/* octave, then raises the normalised value t to CONFIG_ZMK_AMBIENT_  */
/* LIGHT_LUX_GAMMA to bias the curve toward dim at low lux.           */
/*                                                                    */
/* No FPU, no math.h.                                                 */
/* ------------------------------------------------------------------ */

/*
 * Compute log2 of v in 8.8 fixed point (whole part in bits [15:8],
 * fractional part in bits [7:0]).  Returns 0 for v <= 1.
 */
static uint32_t log2_fp8(uint32_t v)
{
    if (v <= 1U) return 0U;

    /* Find floor(log2(v)) */
    uint32_t tmp = v;
    uint8_t whole = 0;
    while (tmp > 1U) { tmp >>= 1; whole++; }

    /* Interpolate within the octave [2^whole, 2^(whole+1)] */
    uint32_t lower = (1U << whole);
    uint32_t upper = (1U << (whole + 1));
    uint8_t  frac  = (uint8_t)(((v - lower) * 256U) / (upper - lower));

    return ((uint32_t)whole << 8) | frac;
}

uint8_t zmk_ambient_light_lux_to_brt(uint32_t lux, uint8_t min_pct,
                                      uint8_t max_pct, uint32_t lux_max)
{
    if (lux == 0 || min_pct >= max_pct) return min_pct;

    uint32_t clamped     = MIN(lux, lux_max);
    uint32_t log2_lux    = log2_fp8(MAX(clamped, 1U));
    uint32_t log2_max    = log2_fp8(MAX(lux_max,  1U));

    if (log2_max == 0U) return max_pct;

    /* t256 ∈ [0, 256]: normalised position on the log2 curve */
    uint32_t t256 = MIN((log2_lux * 256U) / log2_max, 256U);

    /* Raise t to the gamma power using repeated multiplication.
     * Each step keeps the value scaled to [0, 256]. */
    uint32_t t_g = t256;
    for (uint8_t i = 1; i < CONFIG_ZMK_AMBIENT_LIGHT_LUX_GAMMA; i++) {
        t_g = (t_g * t256) / 256U;
    }

    return (uint8_t)(min_pct + (t_g * (uint32_t)(max_pct - min_pct)) / 256U);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

uint32_t zmk_ambient_light_get_lux(void) { return current_lux; }

int zmk_ambient_light_register_cb(zmk_ambient_light_cb_t cb)
{
    if (!cb) return -EINVAL;
    for (uint8_t i = 0; i < cb_count; i++) {
        if (callbacks[i] == cb) return 0; /* already registered */
    }
    if (cb_count >= CONFIG_ZMK_AMBIENT_LIGHT_MAX_CALLBACKS) return -ENOMEM;
    callbacks[cb_count++] = cb;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Polling work                                                        */
/* ------------------------------------------------------------------ */

static void als_work_handler(struct k_work *work)
{
    if (!device_is_ready(als_dev)) {
        LOG_WRN("ALS not ready, skipping");
        goto reschedule;
    }

    struct sensor_value val;
    if (sensor_sample_fetch(als_dev) != 0 ||
        sensor_channel_get(als_dev, SENSOR_CHAN_LIGHT, &val) != 0) {
        LOG_WRN("ALS read failed");
        goto reschedule;
    }

    uint32_t new_lux = (uint32_t)val.val1;
    if (new_lux != current_lux) {
        LOG_DBG("lux %u -> %u", current_lux, new_lux);
        current_lux = new_lux;
        for (uint8_t i = 0; i < cb_count; i++) callbacks[i](current_lux);
    }

reschedule:
    k_work_reschedule(&als_work, K_MSEC(CONFIG_ZMK_AMBIENT_LIGHT_POLL_INTERVAL_MS));
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

static int zmk_ambient_light_init(void)
{
    if (!device_is_ready(als_dev)) {
        LOG_ERR("ALS device '%s' not ready", als_dev->name);
        return -ENODEV;
    }
    LOG_INF("Ambient light: %s", als_dev->name);
    k_work_init_delayable(&als_work, als_work_handler);
    k_work_schedule(&als_work, K_MSEC(CONFIG_ZMK_AMBIENT_LIGHT_STARTUP_DELAY_MS));
    return 0;
}

SYS_INIT(zmk_ambient_light_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
