#include "hw/flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct avr_flash
{
	uint32_t end;
	uint32_t progend;
	uint8_t *data;
};

flash_t *
flash_init(uint32_t end)
{
	flash_t *flash = malloc(sizeof *flash);
	if (flash)
	{
		flash->end = end;
		flash->data = calloc(end + 1, 1);
	}

	return flash;
}

void
flash_destroy(flash_t *flash)
{
	if (flash)
	{
		free(flash->data);
		free(flash);
	}
}

int
flash_upload(flash_t *flash, chunk_t *chunks, size_t n)
{
	int result = 0;

	if (n < 1)
		return -1;

	uint32_t progend = chunks[n-1].baseaddr + chunks[n-1].size - 1;
	if (progend > flash->end)
		return -1;

	/* TODO: This can be improved with memtrack */
	flash->progend = progend;

	for (size_t i = 0; i < n; i++)
	{
		chunk_t chunk = chunks[i];
		uint32_t baseaddr = chunk.baseaddr;

		// TODO if (baseaddr + chunk.size > flash->end + 1)

		memcpy(&flash->data[baseaddr], chunk.data, chunk.size);

		result += chunk.size;
	}

	return result;
}

void
flash_dump(flash_t *flash, uint32_t from, uint32_t to)
{
}

void
flash_write(flash_t *flash, uint32_t addr, uint8_t val)
{
}

uint8_t
flash_read_byte(flash_t *flash, uint32_t addr)
{
	if (addr > flash->end)
	{
		fprintf(stderr, "error: attempted to read beyond data memory 0x%08X (%d)\n", addr, addr);
		addr %= flash->end;
		fprintf(stderr, " => wrapping to 0x%08X (%d)\n", addr, addr);
	}
	else if (addr > flash->progend)
	{
		printf("warning: reading beyond programmed flash (%X)\n", addr);
	}

	return flash->data[addr];
}

uint16_t
flash_read_word(flash_t *flash, uint32_t addr)
{
	uint16_t result = 0;

	/* TODO: Reconsider this */
	addr *= 2;

	// TODO
	uint8_t rl = flash_read_byte(flash, addr);
	uint8_t rh = flash_read_byte(flash, addr+1);

	result = (rh << 8) | rl;

	return result;
}
