#include "hw/devices.h"
#define _AVR_IO_H_
#include "hw/atmel/iom328p.h"
#include "util/logging.h"
#include "util/bitmanip.h"

#include <stdlib.h>
#include <string.h>

hw_t atmega328p_init(void);
static void p_destroy(hw_t *hw);

static hw_t p_atmega328p =
{
	.name = "ATmega328P",
	.signature = { 0x1E, 0x95, 0x14 },

	.pc = 0,

	.sp = NULL,
	.regs = NULL,
	.io = NULL,

	.sreg = { 0 },
	.fuse = { 0xFF, 0xDA, 0x05 },
	.lockbits = 0,

	.e2end = 0,
	.eeprom = NULL,

	.flash = NULL,
	.flashend = FLASHEND,

	.ramstart = RAMSTART,
	.ramend = RAMEND,
	.data = NULL,

	.state = AVR_NORMAL,

	.init = atmega328p_init,
	.destroy = p_destroy,
};

hw_t
atmega328p_init(void)
{
	hw_t hw = p_atmega328p;

	hw.flash = calloc(hw.flashend+1, 1);

	hw.data = calloc(hw.ramend+1, 1);
	hw.regs = hw.data;
	hw.io = IO2MEM(hw.regs);
	hw.sp = hw.io + 0x3D;
	hw.sp[0] = LOW(RAMEND); // Datasheet P14
	hw.sp[1] = HIGH(RAMEND);

	return hw;
}

void
p_destroy(hw_t *hw)
{
	if (!hw)
		PANIC("Bad call to %s", __FUNCTION__ );

	free(hw->flash);
	free(hw->data);
	hw->flash = NULL;
	hw->data = NULL;
}
