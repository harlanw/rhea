#include "hw/devices.h"

hw_t *atmega328p_init();

hw_t *
device_by_name(const char *mcu)
{
	hw_t *hw = atmega328p_init();

	return hw;
}
