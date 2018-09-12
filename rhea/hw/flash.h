#ifndef HW_FLASH_H
#define HW_FLASH_H

#include "rhea_load.h"

#include <stddef.h>
#include <stdint.h>

typedef struct avr_flash flash_t;

flash_t *
flash_init(uint32_t end);

void
flash_destroy(flash_t *flash);

int
flash_upload(flash_t *flash, chunk_t *chunks, size_t n);

void
flash_dump(flash_t *flash, uint32_t from, uint32_t to);

void
flash_write(flash_t *, uint32_t, uint8_t);

uint8_t
flash_read_byte(flash_t *, uint32_t);

uint16_t
flash_read_word(flash_t *, uint32_t);

#endif
