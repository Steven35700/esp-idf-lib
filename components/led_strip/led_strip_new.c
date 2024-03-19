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
 * @file led_strip_new.c
 *
 * RMT-based ESP-IDF driver for WS2812B/SK6812/APA106/SM16703/PI33TB LED strips
 *
 * Copyright (c) 2020 Ruslan V. Uss <unclerus@gmail.com>
 *
 * MIT Licensed as described in the file LICENSE
 */

#include <esp_attr.h>
#include <esp_log.h>
#include <ets_sys.h>
#include <stdlib.h>

#include "led_strip.h"
#include "led_strip_encoder.h"

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

#define COLOR_SIZE(strip) (3 + ((strip)->is_rgbw != 0))

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us

#define RMT_DEFAULT_CONFIG_TX(gpio)                 \
{                                                   \
    .clk_src = RMT_CLK_SRC_DEFAULT,                 \
    .gpio_num = gpio,                               \
    .mem_block_symbols = 64,                        \
    .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,   \
    .trans_queue_depth = 4,                         \
}

static const char *TAG = "led_strip_new";

static rmt_bytes_encoder_config_t rmt_items[LED_STRIP_TYPE_MAX] = { 0 };
static rmt_symbol_word_t rmt_rst_items[LED_STRIP_TYPE_MAX] = { 0 };

typedef enum {
    ORDER_GRB,
    ORDER_RGB,
} color_order_t;

typedef struct {
    uint32_t t0h, t0l, t1h, t1l, trst;
    color_order_t order;
} led_params_t;

static const led_params_t led_params[] = {
    [LED_STRIP_WS2812]  = { .t0h = 400, .t0l = 1000, .t1h = 1000, .t1l = 400, .trst = 50, .order = ORDER_GRB },
    [LED_STRIP_SK6812]  = { .t0h = 300, .t0l = 900,  .t1h = 600,  .t1l = 600, .trst = 80, .order = ORDER_GRB },
    [LED_STRIP_APA106]  = { .t0h = 350, .t0l = 1360, .t1h = 1360, .t1l = 350, .trst = 50, .order = ORDER_RGB },
    [LED_STRIP_SM16703] = { .t0h = 300, .t0l = 900,  .t1h = 1360, .t1l = 350, .trst = 80, .order = ORDER_RGB },
    [LED_STRIP_PI33TB]  = { .t0h = 300, .t0l = 900,  .t1h = 900,  .t1l = 300, .trst = 80, .order = ORDER_GRB },
};

///////////////////////////////////////////////////////////////////////////////

void led_strip_install()
{
    float ratio = (float)RMT_LED_STRIP_RESOLUTION_HZ / 1e09f;

    for (size_t i = 0; i < LED_STRIP_TYPE_MAX; i++) {
        // bit0
        rmt_items[i].bit0.duration0 = (uint32_t)(ratio * led_params[i].t0h);
        rmt_items[i].bit0.level0 = 1;
        rmt_items[i].bit0.duration1 = (uint32_t)(ratio * led_params[i].t0l);
        rmt_items[i].bit0.level1 = 0;
        // bit1
        rmt_items[i].bit1.duration0 = (uint32_t)(ratio * led_params[i].t1h);
        rmt_items[i].bit1.level0 = 1;
        rmt_items[i].bit1.duration1 = (uint32_t)(ratio * led_params[i].t1l);
        rmt_items[i].bit1.level1 = 0;
        // MSB
        rmt_items[i].flags.msb_first = 1;
        // Reset
        rmt_rst_items[i].level0 = 0;
        rmt_rst_items[i].duration0 = (uint32_t)(ratio * led_params[i].trst) / 2;
        rmt_rst_items[i].level1 = 0;
        rmt_rst_items[i].duration1 = (uint32_t)(ratio * led_params[i].trst) / 2;
    }
}

esp_err_t led_strip_init(led_strip_t *strip)
{
    CHECK_ARG(strip && strip->length > 0 && strip->type < LED_STRIP_TYPE_MAX);

    strip->buf = calloc(strip->length, COLOR_SIZE(strip));
    if (!strip->buf) {
        ESP_LOGE(TAG, "not enough memory");
        return ESP_ERR_NO_MEM;
    }

    // Create RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = RMT_DEFAULT_CONFIG_TX(strip->gpio);
    CHECK(rmt_new_tx_channel(&tx_chan_config, &strip->channel));

    // Initializes the led strip encoder
    CHECK(led_strip_encoder_init(rmt_items[strip->type], rmt_rst_items[strip->type],
                                 &strip->encoder));

    // Enable RMT TX channel
    CHECK(rmt_enable(strip->channel));

    return ESP_OK;
}

esp_err_t led_strip_free(led_strip_t *strip)
{
    CHECK_ARG(strip && strip->buf);
    free(strip->buf);

    CHECK(rmt_del_channel(strip->channel));
    CHECK(strip->encoder->del(strip->encoder));

    return ESP_OK;
}

esp_err_t led_strip_flush(led_strip_t *strip)
{
    CHECK_ARG(strip && strip->buf);

    static rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    CHECK(rmt_tx_wait_all_done(strip->channel, pdMS_TO_TICKS(CONFIG_LED_STRIP_FLUSH_TIMEOUT)));
    ets_delay_us(CONFIG_LED_STRIP_PAUSE_LENGTH);
    return rmt_transmit(strip->channel, strip->encoder, strip->buf,
                        strip->length * COLOR_SIZE(strip), &tx_config);
}

bool led_strip_busy(led_strip_t *strip)
{
    if (!strip) return false;
    
    return rmt_tx_wait_all_done(strip->channel, 0) == ESP_ERR_TIMEOUT;
}

esp_err_t led_strip_wait(led_strip_t *strip, TickType_t timeout)
{
    CHECK_ARG(strip);

    return rmt_tx_wait_all_done(strip->channel, timeout);
}

esp_err_t led_strip_set_pixel(led_strip_t *strip, size_t num, rgb_t color)
{
    CHECK_ARG(strip && strip->buf && num <= strip->length);
    size_t idx = num * COLOR_SIZE(strip);
    switch (led_params[strip->type].order)
    {
        case ORDER_GRB:
            strip->buf[idx] = color.g;
            strip->buf[idx + 1] = color.r;
            strip->buf[idx + 2] = color.b;
            if (strip->is_rgbw)
                strip->buf[idx + 3] = rgb_luma(color);
            break;
        case ORDER_RGB:
            strip->buf[idx] = color.r;
            strip->buf[idx + 1] = color.g;
            strip->buf[idx + 2] = color.b;
            if (strip->is_rgbw)
                strip->buf[idx + 3] = rgb_luma(color);
            break;
    }
    return ESP_OK;
}

esp_err_t led_strip_set_pixels(led_strip_t *strip, size_t start, size_t len, rgb_t *data)
{
    CHECK_ARG(strip && strip->buf && len && start + len <= strip->length);
    for (size_t i = 0; i < len; i++)
        CHECK(led_strip_set_pixel(strip, i + start, data[i]));
    return ESP_OK;
}

esp_err_t led_strip_fill(led_strip_t *strip, size_t start, size_t len, rgb_t color)
{
    CHECK_ARG(strip && strip->buf && len && start + len <= strip->length);

    for (size_t i = start; i < start + len; i++)
        CHECK(led_strip_set_pixel(strip, i, color));
    return ESP_OK;
}
