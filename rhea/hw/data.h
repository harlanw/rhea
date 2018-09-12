#ifndef HW_DATA_H
#define HW_DATA_H

#include <stdint.h>

#define IO2MEM(addr) ((addr) + 0x20)

#define SPL IO2MEM(0x3D)
#define SPH IO2MEM(0x3E)

typedef struct avr_data data_t;

data_t *
data_init(uint32_t start, uint32_t end, uint8_t sp[static 2]);

void
data_destroy(data_t *data);

void
data_dump(data_t *data, uint32_t from, uint32_t to);

void
data_write(data_t *data, uint32_t addr, uint8_t val);

void
data_write_word(data_t *data, uint32_t addr, uint16_t val);

uint8_t
data_read(data_t *data, uint32_t addr);

uint16_t
data_read_word(data_t *data, uint32_t addr);

#endif
