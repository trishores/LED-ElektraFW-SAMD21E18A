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

#ifndef TIMER_HANDLER_H_
#define TIMER_HANDLER_H_

extern volatile uint8_t u8ElapsedTicks;

extern void WaitForIntervalElapse();
extern void UsbSofEvent(void);
extern void TimerEvent(const struct timer_task *const timer_task);
extern void TimerAddTask(uint16_t u16TimerIntervalMs);
extern void SetTickInterval(uint16_t timerIntervalMs);
extern void WdtInit(void);

#endif /* TIMER_HANDLER_H_ */