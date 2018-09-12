#ifndef RHEA_LOAD_H
#define RHEA_LOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rhea_file.h"

#include <stdint.h>

typedef enum chunktype { CT_NONE = 0, CT_BINARY, CT_JIT } chunktype_t;

typedef struct chunk
{
	chunktype_t type;
	uint8_t *data;
	uint32_t size;
	uint32_t baseaddr;
} chunk_t;

int rhea_load_file(file_t file, chunk_t **arp);
void rhea_unload_file(file_t file, chunk_t **arp, int n);

#ifdef __cplusplus
};
#endif

#endif
