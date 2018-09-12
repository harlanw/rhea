#include "rhea_ihex.h"

#include "rhea_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LF 10
#define CR 13

#define IHEX_DATA 0x00
#define IHEX_EOF  0x01
#define IHEX_ESA  0x02

int
p_record_to_array(uint8_t buff[], size_t max, char *recd, size_t len)
{
	if (len == 0 || recd[0] != ':' || len > max || (len - 1) & 1)
		return -1;

	--len;
	++recd;

	uint8_t *dst = buff;
	size_t i = 0;
	while (*recd && len != 0)
	{
		char c = *recd++;

		if (c == LF || c == CR)
			continue;

		int byte = hex2bin(c);
		if (byte == -1)
		{
			return -1;
		}

		if (i & 1)
		{
			*dst++ += byte;
			--len;
		}
		else
		{
			*dst = byte << 4;
		}

		++i;
	}

	return dst - buff;
}

int
ihex_load(const char *path, chunk_t **chunks)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
	{
		return -1;
	}

	char *line = NULL;
	size_t n = 0;
	ssize_t len = 0;

	uint32_t segment = 0;
	uint32_t n_chunks = 0;
	uint32_t curr_chunk = 0;

	chunk_t *dst = NULL;

	*chunks = NULL;
	while ((len = getline(&line, &n, fp)) > 0)
	{
		// Convert record (LE, byte string) into uint8_t array
		uint8_t bytes[255];
		int read = p_record_to_array(bytes, sizeof(bytes), line, len);

		// Verify Checksum
		if (read < 4)
		{
			puts("read failed");
			break;
		}

		uint8_t i = read - 1;
		uint8_t *chk = bytes;
		uint8_t chksum = bytes[i];

		while (i--)
			chksum += *chk++;

		if (chksum != 0)
		{
			break;
		}

		uint32_t addr = 0;

		// Parse record
		switch (bytes[3])
		{
			case IHEX_DATA:
				addr = segment | (bytes[1] << 8) | bytes[2];
				break;
			case IHEX_ESA:
				segment = (bytes[4] << 12) | (bytes[5] << 4);
				break;
			case IHEX_EOF:
				continue;
		}

		uint32_t baseaddr = (bytes[2] << 8) + bytes[1];

		if (curr_chunk < n_chunks && dst && addr != dst->baseaddr)
		{
			if (dst->size)
				++curr_chunk;
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

		free(line);
		n = 0;
		line = NULL;
	}

	free(line);
	fclose(fp);

	return n_chunks;
}

void
ihex_unload(chunk_t **chunks, int n)
{
	if (chunks && *chunks)
	{
		for (int i = 0; i < n; i++)
			free((*chunks)[i].data);

		free(*chunks);
		*chunks = NULL;
	}
}
