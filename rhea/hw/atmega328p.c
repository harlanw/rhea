#include "hw/devices.h"

#include "util/bitmanip.h"

#define _AVR_IO_H_
#include "hw/atmel/iom328p.h"

#include <stdlib.h>
#include <string.h>

static void
p_destroy(hw_t **hw)
{
	hw_t *_hw = *hw;

	if (_hw)
	{
		flash_destroy(_hw->flash);
		data_destroy(_hw->data);
		free(_hw);
		*hw = NULL;
	}
}

hw_t *
atmega328p_init(void)
{
	hw_t *hw = malloc(sizeof *hw);

	if (hw)
	{
		memset(hw, 0, sizeof *hw);

		hw->name = "ATmega328P";

		hw->sp[0] = LOW(RAMEND);
		hw->sp[1] = HIGH(RAMEND);

		hw->flash = flash_init(FLASHEND);
		hw->flashend = FLASHEND;
		hw->data = data_init(RAMSTART, RAMEND, hw->sp);
		hw->ramend = RAMEND;

		hw->state = AVR_NORMAL;

		hw->destroy = p_destroy;
	}

	return hw;
}
