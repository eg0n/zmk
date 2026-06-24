/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * app/include/zmk/ambient_light.h
 */

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Latest ambient lux reading (0 until first sample). */
uint32_t zmk_ambient_light_get_lux(void);

/**
 * Map lux to a brightness percent using an integer log2 curve.
 * No floating-point required.
 *
 * @param lux       Current lux reading.
 * @param min_pct   Brightness percent at lux == 0.
 * @param max_pct   Brightness percent at lux >= lux_max.
 * @param lux_max   Lux ceiling (tune for your sensor placement).
 */
uint8_t zmk_ambient_light_lux_to_brt(uint32_t lux, uint8_t min_pct,
                                      uint8_t max_pct, uint32_t lux_max);

typedef void (*zmk_ambient_light_cb_t)(uint32_t lux);

/**
 * Register a callback invoked after each sensor sample.
 * Up to CONFIG_ZMK_AMBIENT_LIGHT_MAX_CALLBACKS may be registered.
 */
int zmk_ambient_light_register_cb(zmk_ambient_light_cb_t cb);

#ifdef __cplusplus
}
#endif
