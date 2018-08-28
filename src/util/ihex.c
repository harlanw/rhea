#include "util/ihex.h"

#include "util/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IHEX_DATA	0x00
#define IHEX_EOF	0x01
#define IHEX_ESA	0x02

#define LF 10
#define CR 13

static size_t hex2bin(char c, uint8_t *bin)
{
	uint8_t result;

	switch (c)
	{
		case '0' ... '9':
			result = c - '0';
			break;
		case 'a' ... 'f':
			result = c - 'a' + 0x0A;
			break;
		case 'A' ... 'F':
			result = c - 'A' + 0x0A;
			break;
		default:
			return -1;
	}

	*bin = result;

	return 0;
}

static uint32_t read_record(const char *src, uint8_t *buffer, size_t n)
{
	size_t i = 0;
	uint8_t *dst = buffer;

	while (*src && n)
	{
		char c = *(src++);
		uint8_t byte;

		if (c == LF || c == CR)
			continue;

		if (hex2bin(c, &byte))
		{
			return 0;
		}

		if (i & 1)
		{
			*(dst++) += byte;
			--n;
		}
		else
		{
			*dst = (byte << 4);
		}

		++i;
	}

	return dst - buffer;
}

uint32_t
avr_load(const char *path, chunk_t **chunks)
{
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp)
	{
		return 0;
	}

	char record[523]; // TODO: Is this really the max length?
	uint32_t segment = 0;
	uint32_t n_chunks = 0, curr_chunk = 0;

	chunk_t *dst = NULL;
	while (fgets(record, sizeof(record), fp))
	{
		uint8_t bytes[256];
		uint32_t len = 0;

		if (record[0] != ':')
		{
			PANIC("Invalid record delimiter ('%c')", record[0]);
		}

		// NOTICE: There is no indication that the IHEX format
		// specificies that the data record is the same endianness as
		// the host system. Currently both are LE on my LE system.
		len = read_record(record + 1, bytes, sizeof(bytes));

		if (len < 4)
		{
			PANIC("Record too short ('%s')", strtok(record, "\r"));
		}

		uint8_t *chk_b = bytes;
		uint32_t chk_i = len - 1;
		uint8_t chk_sum = bytes[chk_i];

		while (chk_i--)
		{
			chk_sum += *(chk_b++);
		}

		if (chk_sum != 0)
		{
			PANIC("Record checksum failed ('%s')", strtok(record, "\r"));
		}

		uint32_t addr = 0;
		switch (bytes[3])
		{
			case IHEX_DATA:
			{
				addr = segment | (bytes[1] << 8) | bytes[2];
				break;
			}
			// Extended Segment Address - It appears this address is
			// always big endian.
			case IHEX_ESA:
			{
				segment = (bytes[4] << 12) | (bytes[5] << 4);
				continue;
			}
			case IHEX_EOF:
			{
				continue;
			}
		}

		uint32_t baseaddr = (bytes[2] << 8) + bytes[1];

		if (curr_chunk < n_chunks && dst && addr != dst->baseaddr)
		{
			if (dst->size)
			{
				++curr_chunk;
			}
		}

		if (curr_chunk >= n_chunks)
		{
			++n_chunks;
			*chunks = realloc(*chunks, n_chunks * sizeof **chunks);

			dst = &(*chunks)[curr_chunk];
			dst->size = 0;
			dst->data = NULL;
			dst->baseaddr = addr;
		}

		dst->data = realloc(dst->data, dst->size + bytes[0]);
		memcpy(dst->data + dst->size, bytes + 4, bytes[0]);
		dst->size += bytes[0];
	}
	fclose(fp);

	return n_chunks;
}

void
avr_unload(chunk_t **chunks, uint32_t n)
{
	for (uint32_t i = 0; i < n; i++)
		free((*chunks)[i].data);

	free(*chunks);

	*chunks = NULL;
}
