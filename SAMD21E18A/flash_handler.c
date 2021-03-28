#include "driver_init.h"

void FlashRead(uint32_t src_addr, uint8_t *buffer, uint32_t length)
{
	flash_read(&FLASH_0, src_addr, buffer, length);    // read first 14 bytes.
}