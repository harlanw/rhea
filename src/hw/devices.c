#include "hw/devices.h"

hw_t atmega328p_init(void);
hw_t DEVICE_LIST[] =
{
	{ .name = "ATmega328P", .init = atmega328p_init }
};

size_t N_DEVICES = sizeof(DEVICE_LIST) / sizeof(DEVICE_LIST[0]);
