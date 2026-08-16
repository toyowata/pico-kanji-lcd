/**
 * Copyright (c) 2020-2025 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 
#include <stdio.h>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/uart.h"
#include "hardware/spi.h"

#include "GT20L16J1Y_font.h"

#define FONT_CS_PIN 13
#define LCD_CS_PIN 17
#define LCD_RS_PIN 20
#define LCD_WIDTH 128
#define LCD_HEIGHT 48

GT20L16J1Y_FONT font(11, 12, 10, FONT_CS_PIN); // mosi, miso, sclk, cs for spi1

unsigned short kbuf1[] = {
    0x9069,
    0x92BB,
    0x82C7,
    0x82A4,
    0x82C5,
    0x82B7,
    0x82A9,
    0x8148
};

unsigned short kbuf2[] = {
    0x93fa,
    0x967b,
    0x8cea,
    0x955c,
    0x8ea6
};

int pico_led_init(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_pull_up(PICO_DEFAULT_LED_PIN);
    return PICO_OK;
}

static inline void cs_select(int cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(int cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop");
}

void wr_cmd(unsigned char cmd)
{
    gpio_put(LCD_RS_PIN, 0);
    cs_select(LCD_CS_PIN);
    sleep_us(2);
    spi_write_blocking(spi0, &cmd, 1);
    cs_deselect(LCD_CS_PIN);
}

void wr_dat(unsigned char dat)
{
    gpio_put(LCD_RS_PIN, 1);
    cs_select(LCD_CS_PIN);
    sleep_us(2);
    spi_write_blocking(spi0, &dat, 1);
    cs_deselect(LCD_CS_PIN);
}

void clear_lcd(void) {
    // Use a static buffer to avoid stack allocation and memset on every call.
    // The buffer is initialized to zeros only once.
    static const uint8_t zeros[LCD_WIDTH] = {0};

    for (int i = 0; i < (LCD_HEIGHT / 8); i++) {
        // Group commands to send them in a single SPI transaction for efficiency.
        uint8_t cmds[] = {
            0x00,                // set column low nibble 0
            0x10,                // set column hi nibble 0
            (uint8_t)(0xB0 + i)  // set page address
        };
        gpio_put(LCD_RS_PIN, 0);
        cs_select(LCD_CS_PIN);
        sleep_us(2);
        spi_write_blocking(spi0, cmds, sizeof(cmds));
        cs_deselect(LCD_CS_PIN);

        // Write a full line of zeros in data mode.
        gpio_put(LCD_RS_PIN, 1);
        cs_select(LCD_CS_PIN);
        sleep_us(2);
        spi_write_blocking(spi0, zeros, sizeof(zeros));
        cs_deselect(LCD_CS_PIN);
    }
}

void draw_kanji(int x, int y) {
    wr_cmd(0x00 + (x & 0x0f));
    wr_cmd(0x10 + ((x >> 4) & 0x0f));
    wr_cmd(0xB0 + y);
    gpio_put(LCD_RS_PIN, 1);
    cs_select(LCD_CS_PIN);
    sleep_us(2);
    spi_write_blocking(spi0, &font.bitmap[0], 16);
    cs_deselect(LCD_CS_PIN);

    wr_cmd(0x00 + (x & 0x0f));
    wr_cmd(0x10 + ((x >> 4) & 0x0f));
    wr_cmd(0xB0 + y + 1);
    gpio_put(LCD_RS_PIN, 1);
    cs_select(LCD_CS_PIN);
    sleep_us(2);
    spi_write_blocking(spi0, &font.bitmap[16], 16);
    cs_deselect(LCD_CS_PIN);
}

/**
 * @brief LCDにShift-JISエンコードされた文字列を描画します。
 *
 * @param x 開始X座標（ピクセル単位）
 * @param y 開始Y座標（ページ単位、1ページ=8ピクセル）
 * @param str 描画するShift-JISエンコードされた文字列
 * @note 文字列リテラルを正しく表示するには、ソースファイルをShift-JISエンコーディングで保存する必要があります。
 */
void draw_string(int x, int y, const char* str) {
    int current_x = x;
    int current_y = y;
    unsigned short sjis_code;

    while (*str) {
        // 現在位置が画面幅以上の場合、次の行に折り返す
        if (current_x >= LCD_WIDTH) {
            current_x = 0;
            current_y += 2; // 2ページ（16ピクセル）下に移動
        }

        // 画面の下端を超えたら描画を停止
        if (current_y >= (LCD_HEIGHT / 8)) {
            break;
        }

        unsigned char c1 = *str++;
        // 2バイトのShift-JIS文字かチェック
        if ((c1 >= 0x81 && c1 <= 0x9f) || (c1 >= 0xe0 && c1 <= 0xef)) {
            unsigned char c2 = *str++;
            if (c2 == '\0') { break; }
            sjis_code = (c1 << 8) | c2;
        } else {
            sjis_code = c1;
        }
        font.read(sjis_code);
        draw_kanji(current_x, current_y);
        current_x += 16; // 次の文字のためにX座標を進める
    }
}

int pico_spi_init(void) {
    // This example will use SPI0 at 2MHz.
    // LCD SPI setup
    spi_init(spi0, 2000 * 1000);
    spi_set_format(spi0, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(18, GPIO_FUNC_SPI);
    gpio_set_function(19, GPIO_FUNC_SPI);
    gpio_set_function(16, GPIO_FUNC_SPI);
    gpio_set_function(LCD_CS_PIN, GPIO_FUNC_SIO);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(LCD_CS_PIN);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT);
    gpio_put(LCD_CS_PIN, 1);
    
    // LCD RS pin
    gpio_init(LCD_RS_PIN);
    gpio_set_dir(LCD_RS_PIN, GPIO_OUT);
    gpio_put(LCD_RS_PIN, 0);

    wr_cmd(0xAE);   // display off
    wr_cmd(0xA0);
    wr_cmd(0xC8);   // colum normal
    wr_cmd(0xA3);   // bias voltage
    wr_cmd(0x2c); sleep_ms(2);
    wr_cmd(0x2e); sleep_ms(2);
    wr_cmd(0x2F);   // power on
    wr_cmd(0x23);
    wr_cmd(0x81);
    wr_cmd(0x1c);
    wr_cmd(0xA4);   // LCD display ram
    wr_cmd(0x40);   // start line = 0
    wr_cmd(0xa6);
    wr_cmd(0xAF);   // display ON

    clear_lcd();

    return PICO_OK;
}

void pico_set_led(bool led_on) {
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

int main() {
    bool blink = true;

    //stdio_init_all();
    pico_led_init();
    pico_spi_init();

    int kbuf_size;
    kbuf_size = sizeof(kbuf1) / sizeof(kbuf1[0]);
    for(int i=0; i<kbuf_size; i++) {
        font.read(kbuf1[i]);
        draw_kanji(i*16, 0);
    }
    kbuf_size = sizeof(kbuf2) / sizeof(kbuf2[0]);
    for(int i=0; i<kbuf_size; i++) {
        font.read(kbuf2[i]);
        draw_kanji(i*16, 2);
    }

    // 文字列リテラルを正しく表示するには、ソースファイルをShift-JISエンコーディングで保存する必要があります。
    // draw_string(0, 0, "Picoで漢字表示");
    // draw_string(0, 2, "こんにちは世界！");

    while (1) {
        pico_set_led(blink);
        blink = !blink;
        sleep_ms(500);
    }

    return 0;
}
