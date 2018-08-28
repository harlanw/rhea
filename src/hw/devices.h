#ifndef RHEA_HARDWARE_H
#define RHEA_HARDWARE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util/ihex.h"

#include <stdint.h>
#include <stddef.h>

#define R0 0
#define R1 1

#define SREG_I 7
#define SREG_T 6
#define SREG_H 5
#define SREG_S 4
#define SREG_V 3
#define SREG_N 2
#define SREG_Z 1
#define SREG_C 0

#define GET_SREG(hw,b,v) ((hw)->sreg[(b)])
#define SET_SREG(hw,b,v) ((hw)->sreg[(b)] = ((v)&0x1))

#define XL 26
#define XH 27
#define X XL

#define YL 28
#define YH 29
#define Y YL

#define ZL 30
#define ZH 31
#define Z ZL

#define IO2MEM(addr) ((addr) + 0x20)
#define FETCH_WORD(ptr, offs) \
	((ptr)[offs] | ((ptr)[offs + 1] << 8))

enum avr_state { AVR_NORMAL = 0, AVR_SLEEP, AVR_BREAK };

typedef enum avr_state avr_state_t;

struct avr_hardware
{
	const char *name;
	uint8_t signature[3];

	uint32_t pc;

	/* Point to location in data field (opposite AVR */
	uint8_t *io;
	uint8_t *regs;
	uint8_t *sp;

	uint8_t sreg[8];
	uint8_t fuse[3];
	uint8_t lockbits;

	uint16_t e2end;
	uint8_t *eeprom;

	uint16_t flashend;
	uint8_t *flash;

	uint16_t ramstart;
	uint16_t ramend;
	uint8_t *data;

	avr_state_t state;

	struct avr_hardware (*init)(void);
	void (*destroy)(struct avr_hardware *hw);
};

typedef struct avr_hardware hw_t;

extern hw_t DEVICE_LIST[];
extern size_t N_DEVICES;

#ifdef __cplusplus
};
#endif

#endif
