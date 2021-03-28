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

#include "driver_init.h"

static struct timer_task _structTimer0Task;
static uint16_t _u16SofMsCounter;

static uint16_t _tickIntervalMs;
static volatile uint8_t u8ElapsedTicks; // volatile critical.
//static bool level;  // debug.

void WaitForIntervalElapse()
{
    while(u8ElapsedTicks == 0) continue;    // wait until a minimum of 1-tick has elapsed.
    u8ElapsedTicks = 0;   // reset tick counter.
}

// SOF interrupt is triggered on receipt of sof frame from usb host (every 1ms).
void UsbSofEvent(void)
{
	if (++_u16SofMsCounter > _tickIntervalMs)
	{
		_u16SofMsCounter = 0;
		u8ElapsedTicks++;

		//gpio_set_pin_level(EXT_LED_DATA_PIN, level = !level);    // debugging.
	}
}

// Timer should only run when sof is down (no usb host connection).
void TimerEvent(const struct timer_task *const timer_task)
{
	u8ElapsedTicks++;
	(void)timer_task;

	//gpio_set_pin_level(EXT_LED_DATA_PIN, level = !level);    // debugging.
}

void TimerAddTask(uint16_t timerIntervalMs)
{
	_structTimer0Task.interval = timerIntervalMs;
	_structTimer0Task.cb       = TimerEvent;
	_structTimer0Task.mode     = TIMER_TASK_REPEAT;
	timer_add_task(&TIMER_0, &_structTimer0Task);
}

void SetTickInterval(uint16_t timerIntervalMs)
{
    _tickIntervalMs = timerIntervalMs;
    _structTimer0Task.interval = timerIntervalMs;
}

void WdtInit(void)
{
	uint32_t wdtClkFreq;
	uint16_t timeoutPeriodMs;
	wdtClkFreq = 32768;
	timeoutPeriodMs = 500;
	wdt_set_timeout_period(&WDT_0, wdtClkFreq, timeoutPeriodMs);
	wdt_enable(&WDT_0);
}
