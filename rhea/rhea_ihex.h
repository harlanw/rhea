#ifndef RHEA_IHEX_H
#define RHEA_IHEX_H

#include "rhea_load.h"

int
ihex_load(const char *path, chunk_t **arp);

void
ihex_unload(chunk_t **chunks, int n);

#endif
