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
#include "usb_start.h"
#include "atmel_start_pins.h"
#include "ledstrip_driver.h"
#include "timer_handler.h"

#pragma region Defines

#define ON 1
#define OFF 0
#define Pc2Dev_Control 0

#pragma endregion

#pragma region Definitions/declarations

static uint8_t u8SramBuffer[SRAM_BUF_SZ];
uint8_t *ptrSramBufferStart = u8SramBuffer;
uint8_t *ptrSram;
uint32_t ptrNvm;

enum AnimationFlag
{
    Stop = 0,
    RunInit = 1,
    Run = 2
};
static volatile enum AnimationFlag animationFlag;	// volatile keyword is critical.

enum PacketFlag
{
    ControlFlag = 0,
    StoreFlag = 1,
};
static volatile enum PacketFlag packetFlag;

static struct usbdc_handler _structUsbSofEvent = {NULL, (FUNC_PTR)UsbSofEvent};
static bool isActiveAnimation;
static bool isActiveMemWrite;
static volatile bool isSaveToRom;

#pragma endregion

#pragma region USB reports

static void UsbInputReportCallback (uint8_t *ptrUsbBuf, uint16_t usbBufLen)
{
	// Check for break-packet (all buffer bytes 0xFF):
	uint8_t i;
	bool isBreakPacket = true;
	for (i = 0; i < usbBufLen; i++) if (ptrUsbBuf[i] != 0xFF) isBreakPacket = false;

    // Handle break packet (halts any current animation and sets controller to listen for control packets):
	if (isBreakPacket)
	{
    	animationFlag = Stop;
		packetFlag = ControlFlag;
		return;
	}

    if (isActiveAnimation || isActiveMemWrite)
    {
        return;  // ignore non-break packets when an animation is running or a memory write is in-progress.
    }

    // Handle control packet:
    if (packetFlag == ControlFlag)
    {
        if ((*ptrUsbBuf >> 4) != Pc2Dev_Control) return;  // check first byte is control instruction.

	    // Parse control instruction:
	    isSaveToRom = *ptrUsbBuf & 0x08;
	    uint8_t ctrlOpcode = *ptrUsbBuf & 0x07;

	    // Executing control instruction...

	    if (ctrlOpcode == 0)    // stop animation.
        {
            animationFlag = Stop;  // redundant since accomplished by break packet.
        }
        else if (ctrlOpcode == 1)    // resume animation.
        {
            animationFlag = Run;
        }
        else if (ctrlOpcode == 2)    // start animation.
        {
            animationFlag = RunInit;
        }
        else if (ctrlOpcode == 3)    // store new packets.
        {
            animationFlag = Stop;  // redundant since accomplished by break packet.
            packetFlag = StoreFlag;

            if (!isSaveToRom)    // store subsequent packets to sram.
            {
                // Sram init:
                ptrSram = ptrSramBufferStart;	// set sram pointer to start of available sram region.
            }
            else if (isSaveToRom) // store to nvm.
            {
                // Nvm init:
                ptrNvm = NVM_BUF_START_ADDR;	// set nvm pointer to start of free flash region.
		        //flash_erase(&FLASH_0, NVM_BUF_START_ADDR, NVM_BUF_PAGES_COUNT);   // only useful if debugging memory.
            }

            // Set default light pattern:
            SetLedstripTestColor(5, 5, 5, 1);
        }
    }

	// Handle storage packet:
	else if (packetFlag == StoreFlag)
	{
    	isActiveMemWrite = true;

        if (!isSaveToRom)    // store to sram.
    	{
        	memcpy(ptrSram, ptrUsbBuf, usbBufLen);	// write packet bytes to instruction buffer.
        	ptrSram += usbBufLen;	// set instruction buffer write pointer to next unfilled buffer byte.
    	}
    	else if (isSaveToRom) // store to nvm.
    	{
		    flash_write(&FLASH_0, ptrNvm, ptrUsbBuf, usbBufLen);
		    ptrNvm += usbBufLen;
        }

        isActiveMemWrite = false;
    }
}

static void UsbOutputReportCallback (uint8_t *usb_buf, uint16_t usb_buffer_len)
{
    (void)usb_buffer_len;
	// Send status flags to host:
    usb_buf[0] = isActiveAnimation;
    usb_buf[1] = isActiveMemWrite;
    usb_buf[2] = packetFlag;
}

#pragma endregion

int main(void)
{
#pragma region Main initialization

	// System initialization
	system_init();

    // Watchdog init:
    WdtInit();

	// Enable led power supply:
	LedPowerInit();

    // Add timer task:
    TimerAddTask(10);    // set arbitrary initial value for tick interval.
    timer_start(&TIMER_0);

    // Initialize usb:
    bool isHidGenericEnabled = false;
    hid_generic_init();
    usbdc_register_handler(USBDC_HDL_SOF, &_structUsbSofEvent);

    // Set default light pattern:
    SetLedstripTestColor(1, 1, 1, 1);

    // Restart animation on boot only if last reset cause
    // was not due to corrupt animation binary data:
    animationFlag = Stop;
    volatile uint8_t rstReason = _get_reset_reason();
    if (rstReason != RESET_REASON_WDT)
	{
		// Initiate boot-up check for an nvm-stored animation (sram is blank after reset):
		isSaveToRom = true;
		animationFlag = RunInit;
	}
    /*
    RESET_REASON_POR   = 1,
	RESET_REASON_BOD12 = 2,
	RESET_REASON_BOD33 = 4,
	RESET_REASON_EXT   = 16,
	RESET_REASON_WDT   = 32,
	RESET_REASON_SYST  = 64,
    */

#pragma endregion

#pragma region Main loop

	while (true)
	{
        // Reset watchdog:
        wdt_feed(&WDT_0);

        // Implement non-blocking hid initialization:
        if (!isHidGenericEnabled && hiddf_generic_is_enabled())
        {
            isHidGenericEnabled = true; // hiddf_generic_is_enabled() always returns true once enabled even if host is disconnected.
            timer_stop(&TIMER_0);   // turn off timer since sof is running.
            hiddf_generic_register_callback(HIDDF_GENERIC_CB_GET_CTRL_REPORT, (FUNC_PTR)UsbInputReportCallback);
            hiddf_generic_register_callback(HIDDF_GENERIC_CB_SET_CTRL_REPORT, (FUNC_PTR)UsbOutputReportCallback);
        }

        if (animationFlag == RunInit)
        {
            isActiveAnimation = true;
	        if (InitAnimation(isSaveToRom))
            {
                animationFlag = Run;
            }
            else
            {
                animationFlag = Stop;
            }
        }
        else if (animationFlag == Run)
        {
            isActiveAnimation = true;

            // Pause until next tick:
            WaitForIntervalElapse();

	        //gpio_set_pin_level(EXT_LED_DATA_PIN, OFF);    // debugging.

            if (!RunAnimation(isSaveToRom))
            {
                animationFlag = Stop;
            }

            //gpio_set_pin_level(EXT_LED_DATA_PIN, ON);    // debugging.
        }

        isActiveAnimation = false;
    }
#pragma endregion
}