#ifndef RHEA_HW_DEVICES_H
#define RHEA_HW_DEVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rhea_load.h"
#include "hw/data.h"
#include "hw/flash.h"

#include <stdint.h>
#include <stddef.h>

#define R0 0
#define R1 1

#define XL 26
#define XH 27
#define X XL

#define YL 28
#define YH 29
#define Y YL

#define ZL 30
#define ZH 31
#define Z ZL

typedef enum avr_state { AVR_NORMAL = 0, AVR_SLEEP, AVR_BREAK } avr_state_t;

typedef struct avr_sreg
{
	unsigned int i: 1;
	unsigned int t: 1;
	unsigned int h: 1;
	unsigned int s: 1;
	unsigned int v: 1;
	unsigned int n: 1;
	unsigned int z: 1;
	unsigned int c: 1;
} sreg_t;

typedef struct avr_fuse
{
	unsigned int ex: 8;
	unsigned int hi: 8;
	unsigned int lo: 8;
} fuse_t;

typedef struct avr_hardware
{
	const char *name;

	uint32_t pc;
	uint8_t sp[2];

	sreg_t sreg;
	fuse_t fuse;
	uint8_t signature[3];

	uint32_t e2end;
	uint8_t *eeprom;

	flash_t *flash;
	data_t *data;

	uint32_t flashend, ramend;

	avr_state_t state;

	void (*destroy)(struct avr_hardware **);
} hw_t;

hw_t *device_by_name(const char *);

#ifdef __cplusplus
};
#endif

#endif
