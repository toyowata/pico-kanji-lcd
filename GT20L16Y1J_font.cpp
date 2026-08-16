/**
 * Copyright (c) 2026 Toyomasa Watarai
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include "GT20L16J1Y_font.h"
#include "hardware/spi.h"


GT20L16J1Y_FONT::GT20L16J1Y_FONT(int mosi, int miso, int sclk, int cs) {
    _spi = spi1;
    _cs_pin = cs;

    spi_init(_spi, 1000 * 1000);
    spi_set_format(_spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(sclk, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);

    gpio_init(_cs_pin);
    gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_put(_cs_pin, 1);
}

void GT20L16J1Y_FONT::cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(_cs_pin, 0);
    asm volatile("nop \n nop \n nop");
}

void GT20L16J1Y_FONT::cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(_cs_pin, 1);
    asm volatile("nop \n nop \n nop");
}

int GT20L16J1Y_FONT::read_kuten(unsigned short code) {
    unsigned char MSB, LSB;
    uint32_t address;
    int ret;
    
    MSB = (code & 0xFF00) >> 8;
    LSB = code & 0x00FF;
    address = 0;
    
    if(     MSB >=  1 && MSB <= 15 && LSB >= 1 && LSB <= 94)
        address =( (MSB -  1) * 94 + (LSB - 1))*32;
    else if(MSB >= 16 && MSB <= 47 && LSB >= 1 && LSB <= 94)
        address =( (MSB - 16) * 94 + (LSB - 1))*32 + 0x0AA40L;
    else if(MSB >= 48 && MSB <= 84 && LSB >= 1 && LSB <= 94)
        address = ((MSB - 48) * 94 + (LSB - 1))*32 + 0x21CE0L;
    else if(MSB == 85 &&                LSB >= 1 && LSB <= 94)
        address = ((MSB - 85) * 94 + (LSB - 1))*32 + 0x3C4A0L;
    else if(MSB >= 88 && MSB <= 89 && LSB >= 1 && LSB <= 94)
        address = ((MSB - 88) * 94 + (LSB - 1))*32 + 0x3D060L;
    else if(MSB == 0 && LSB >= 0x20 && LSB <= 0x7F)
        address = (LSB - 0x20)*16 + 0x3E7E0L; // ASCII
    else if(MSB == 0 && LSB >= 0xA1 && LSB <= 0xDF)
        address = (LSB - 0xA1)*16 + 0x3E060L; // Half-width Katakana
    
    uint8_t tx_buf[36] = {0};
    uint8_t rx_buf[36] = {0};
    tx_buf[0] = 0x03; // Read command
    tx_buf[1] = (address >> 16) & 0xff;
    tx_buf[2] = (address >> 8) & 0xff;
    tx_buf[3] = address & 0xff;

    cs_select();
    spi_write_read_blocking(_spi, tx_buf, rx_buf, 36);
    cs_deselect();

    if(MSB == 0 && ((LSB >= 0x20 && LSB <= 0x7F) || (LSB >= 0xA1 && LSB <= 0xDF))) {
        // ANK Font (8x16), 16 bytes. We read 32 but only use 16.
        for(int i=0; i<16; i++) { 
            bitmap[i*2]   = rx_buf[4 + i]; // Left 8 pixels
            bitmap[i*2+1] = 0x00;          // Right 8 pixels are blank
        }
        ret = 8;
    } else {
        // Full-width font (16x16), 32 bytes
        for(int i=0; i<32; i++) { bitmap[i] = rx_buf[4 + i]; }
        ret = 16;
    }
    
    return ret;
}

void GT20L16J1Y_FONT::read(unsigned short code) {
    unsigned char c1, c2, MSB, LSB;
    uint32_t seq;
    uint16_t kuten_code;
    
    c1 = (code>>8);
    c2 = (code & 0xFF);
    
    // Check for single-byte character (ASCII or half-width Katakana)
    if (c1 == 0) {
        kuten_code = c2;
    } 
    // Check for full-width SJIS
    else {
        // SJIS to kuten code conversion
        seq = (c1 <= 159 ? c1 - 129 : c1 - 193) * 188 + (c2 <= 126 ? c2 - 64 : c2 - 65);
        MSB = seq / 94 + 1;
        LSB = seq % 94 + 1;
        kuten_code = ((MSB << 8) | LSB);
    }
    read_kuten(kuten_code);
}
