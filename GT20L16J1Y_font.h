/**
 * Copyright (c) 2026 Toyomasa Watarai
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "hardware/spi.h"
#include "pico/stdlib.h"

class GT20L16J1Y_FONT {
  public:
    /** Create a GT20L16J1Y font ROM connected to the specified pins
     *
     *  @param mosi Serial data output pin to connect to
     *  @param moso Serial data input pin to connect to
     *  @param sclk Serial clock input pin to connect to
     *  @param cs Chip enable input pin to connect to
     */
    GT20L16J1Y_FONT(int mosi, int miso, int sclk, int cs);

    /** Read font data from SJIS code
     *
     *  @param code Japanese Kanji font code (Shift JIS code)
     */
    void read(unsigned short code);
    
    /** Read font data from Ku-Ten code
     *
     *  @param code Japanese Kanji font code (Kuten code [15:8] Ku, [7:0] Ten)
     *  @return font width (8 or 16)
     */
    int read_kuten(unsigned short code);

    unsigned char bitmap[32];

  private:
    spi_inst_t *_spi;
    int _cs_pin;
    void cs_select();
    void cs_deselect();
};
