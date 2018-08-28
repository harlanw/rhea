#ifndef AVREMU_IHEX_H
#define AVREMU_IHEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct chunk
{
	uint32_t baseaddr;
	uint8_t *data;
	uint32_t size;
};

typedef struct chunk chunk_t;

uint32_t avr_load(const char *path, chunk_t **chunks);
void avr_unload(chunk_t **chunks, uint32_t n);

#ifdef __cplusplus
};
#endif

#endif
