/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Ruslan V. Uss <unclerus@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file led_strip.h
 * @defgroup led_strip led_strip
 * @{
 *
 * RMT-based ESP-IDF driver for WS2812B/SK6812/APA106/SM16703 LED strips
 *
 * Copyright (c) 2020 Ruslan V. Uss <unclerus@gmail.com>
 *
 * MIT Licensed as described in the file LICENSE
 */

#ifndef __LED_STRIP_ENCODER_H__
#define __LED_STRIP_ENCODER_H__

#include <esp_idf_version.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

#include <driver/rmt_encoder.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the LED strip encoder
 *
 * This function initializes the LED strip encoder with the provided byte encoder configuration,
 * reset configuration, and returns the encoder handle
 *
 * @param bytes_config Configuration for the byte encoder
 * @param rst_config Reset configuration
 * @param ret_encoder Pointer to store the handle of the initialized encoder
 * @return `ESP_OK` on success
 */
esp_err_t led_strip_encoder_init(rmt_bytes_encoder_config_t bytes_config, rmt_symbol_word_t rst_config,
                                 rmt_encoder_handle_t *ret_encoder);

#ifdef __cplusplus
}
#endif
#endif

/**@}*/

#endif /* __LED_STRIP_ENCODER_H__ */
