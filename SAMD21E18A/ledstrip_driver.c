/*
 *  Copyright 2018-2021 ledmaker.org
 *
 *  This file is part of Elektra-SAMD21E18A.
 *
 *  Elektra-SAMD21E18A is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License,
 *  or any later version.
 *
 *  Elektra-SAMD21E18A is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Elektra-SAMD21E18A. If not, see https://www.gnu.org/licenses/.
 */

#include "..\..\GlowDecompiler\public_api.h"
#include "driver_init.h"
#include <string.h>
#include "ledstrip_driver.h"

// Uncomment LED sequence to match IC:
#define LED_SEQUENCE_RBG  // adafruit apa102c
//#define LED_SEQUENCE_BGR    // shiji-led apa102c
//#define LED_SEQUENCE_RGB

#define NUL 0

union LedDataFrame
{
    struct DataFrame
    {
#ifdef LED_SEQUENCE_RGB
        uint8_t blue;
        uint8_t green;
        uint8_t red;    // start of sequence.
#endif
#ifdef LED_SEQUENCE_BGR
        uint8_t red;
        uint8_t green;
        uint8_t blue;    // start of sequence.
#endif
#ifdef LED_SEQUENCE_RBG
        uint8_t green;
        uint8_t blue;
        uint8_t red;    // start of sequence.
#endif
        uint8_t bright : 5;
        uint8_t unused : 3;
    } bitmap;
    uint32_t value;
};

static union LedDataFrame _ledDataFrame = { .bitmap.unused = 7 };
static uint32_t _ledStartFrame = 0x00000000;
static uint32_t _ledStopFrame  = 0xFFFFFFFF;

void LedPowerInit()
{
    gpio_set_pin_level(LED_PWR_EN, 1);
}

static void ProgramLedFrame(uint8_t dataPin, uint32_t ledFrame)
{
    bool next_clk_level;
    uint8_t ledFrameBits = 32;

    while (ledFrameBits)
    {
        // toggle clock line:
        next_clk_level = !gpio_get_pin_level(LED_CLK_PIN);
        gpio_set_pin_level(LED_CLK_PIN, next_clk_level);
        if (next_clk_level == 1) continue;
        // toggle data line on clock falling edge:
        bool pinLevel = ledFrame & (1 << --ledFrameBits);
        gpio_set_pin_level(dataPin, pinLevel);
    }
}

// Duration of 3 led strips programming: 3.6ms @ 48MHz cpu clock.
// TODO: Can be faster if all 3 ledstrips are programmed in parallel.
void ProgramLedstrip(struct LedstripBuffer *ledstrip)
{
    ledstrip->isDirty = false;

    // program single led per loop:
    for (int ledIdx = 0; ledIdx < ledstrip->numLeds; ledIdx++)
    {
        _ledDataFrame.bitmap.red = ledstrip->leds[ledIdx].red;
        _ledDataFrame.bitmap.green = ledstrip->leds[ledIdx].green;
        _ledDataFrame.bitmap.blue = ledstrip->leds[ledIdx].blue;
        _ledDataFrame.bitmap.bright = ledstrip->leds[ledIdx].bright;

        // Elektra-specific led programming (translate single abstract ledstrip to the 3 hardware ledstrips):
        if (ledIdx < INNER_LED_COUNT)
        {
            if (ledIdx == 0) ProgramLedFrame(INNER_LED_DATA_PIN, _ledStartFrame);   // program start frame.
            ProgramLedFrame(INNER_LED_DATA_PIN, _ledDataFrame.value);   // program data frame.
            if (ledIdx == INNER_LED_COUNT - 1) ProgramLedFrame(INNER_LED_DATA_PIN, _ledStopFrame); // program stop frame.
        }
        else if (ledIdx < INNER_LED_COUNT + OUTER_LED_COUNT)
        {
            if (ledIdx == INNER_LED_COUNT) ProgramLedFrame(OUTER_LED_DATA_PIN, _ledStartFrame);   // program start frame.
            ProgramLedFrame(OUTER_LED_DATA_PIN, _ledDataFrame.value);   // program data frame.
            if (ledIdx == INNER_LED_COUNT + OUTER_LED_COUNT - 1) ProgramLedFrame(OUTER_LED_DATA_PIN, _ledStopFrame); // program stop frame.
        }
        else if (ledIdx < INNER_LED_COUNT + OUTER_LED_COUNT + EDGE_LED_COUNT)
        {
            if (ledIdx == INNER_LED_COUNT + OUTER_LED_COUNT) ProgramLedFrame(EDGE_LED_DATA_PIN, _ledStartFrame);   // program start frame.
            ProgramLedFrame(EDGE_LED_DATA_PIN, _ledDataFrame.value);   // program data frame.
            if (ledIdx == INNER_LED_COUNT + OUTER_LED_COUNT + EDGE_LED_COUNT - 1) ProgramLedFrame(EDGE_LED_DATA_PIN, _ledStopFrame); // program stop frame.
        }
    }
}

void SaveBrightnessCoefficient(uint16_t brightnessCoeff)
{
    (void)brightnessCoeff;  // unused.
    // Brightness coefficient may optionally be used hard-limit animation peak brightness for eye-safety or thermal considerations.
}    