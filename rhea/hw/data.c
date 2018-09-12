#include "hw/data.h"

#include "util/bitmanip.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct avr_data
{
	uint8_t regs[32];
	uint8_t *io;
	uint8_t *sram;
	uint32_t ramstart;
	uint32_t ramend;

#ifdef USE_MEMTRACK
	uint8_t *membrane;
#endif

	uint8_t **mmap;
};

data_t *
data_init(uint32_t start, uint32_t end, uint8_t sp[static 2])
{
	data_t *data = malloc(sizeof *data);
	if (data)
	{
		data->io = calloc(start - 32, 1);
		data->sram = calloc(end - start + 1, 1);
		data->ramstart = start;
		data->ramend = end;

#ifdef USE_MEMTRACK
		data->membrane = calloc(end + 1, 1);
#endif

		data->mmap = malloc((end + 1) * sizeof *data->mmap);

		/* Technically this is not correct */
		memset(data->regs, 0, 32);
		memset(data->io, 0, start - 32);
		memset(data->sram, 0, end - start + 1);

		/* MMAP -> Register File */
		for (size_t i = 0; i < 32; i++)
			data->mmap[i] = &data->regs[i];

		/* MMAP -> IO & Ext. IO Space */
		for (size_t i = 32; i < start; i++)
			data->mmap[i] = data->io + (i - 32);

		/* MMAP -> SRAM */
		for (size_t i = start; i <= end; i++)
			data->mmap[i] = data->sram + (i - start);

		data->mmap[SPL] = sp;
		data->mmap[SPH] = sp + 1;
	}

	return data;
}

void
data_destroy(data_t *data)
{
	if (data)
	{
		free(data->io);
		free(data->sram);
#ifdef USE_MEMTRACK
		free(data->membrane);
#endif
		free(data->mmap);
		free(data);
	}
}

void
data_dump(data_t *data, uint32_t from, uint32_t to)
{
	for (uint32_t base = from; base <= to; base += 32)
	{
		for (uint32_t offs = 0; offs < 32; offs++)
		{
			uint16_t addr = base + offs;
			if (addr > data->ramend)
				break;
			uint8_t byte = *data->mmap[addr];

			if (offs && !(offs%8))
				printf(" ");

			if ((addr == SPH) || (addr == SPL))
				printf("\e[1m%02X\e[0m", byte);
			else if (byte)
			{
				if (offs%2)
					printf("\e[34m");
				printf("%02X", byte);
				if (offs%2)
					printf("\e[0m");
			}
			else
			{
				printf("..");
			}
		}

		printf(" ");

		for (uint32_t offs = 0; offs < 32; offs++)
		{
			uint16_t addr = base + offs;
			char c = *data->mmap[addr];

			if (!(c >= ' ' && c <= '~'))
				c = '.';

			printf("%c", c);
		}

		printf("\n");
	}
}

void
data_write(data_t *data, uint32_t addr, uint8_t val)
{
	if (addr > data->ramend)
	{
		printf("Attempted to write beyond data memory 0x%04X (%d)\n", addr, addr);
		addr %= data->ramend;
		printf(" => wrapping to 0x%04X (%d)\n", addr, addr);
	}

#ifdef USE_MEMTRACK
	++data->membrane[addr];
#endif

	*data->mmap[addr] = val;
}

void
data_write_word(data_t *data, uint32_t addr, uint16_t val)
{
	const uint8_t lo = LOW(val);
	const uint8_t hi = HIGH(val);

	data_write(data, addr, lo);
	data_write(data, addr + 1, hi);
}

uint8_t
data_read(data_t *data, uint32_t addr)
{
	if (addr > data->ramend)
	{
		printf("Attempted to read beyond data memory 0x%04X (%d)\n", addr, addr);
		addr %= data->ramend;
		printf(" => wrapping to 0x%04X (%d)\n", addr, addr);
	}

#ifdef USE_MEMTRACK
	if (addr >= data->ramstart && data->membrane[addr] == 0)
	{
		printf("warning: reading from uninitialized data memory (0x%04X)\n", addr);
	}
#endif

	return *data->mmap[addr];
}

uint16_t
data_read_word(data_t *data, uint32_t addr)
{
	uint16_t result = 0;

	uint8_t rl = data_read(data, addr);
	uint8_t rh = data_read(data, addr + 1);

	result = (rh << 8) | rl;

	return result;
}
