#ifndef RHEA_EMU_H
#define RHEA_EMU_H

#include "rhea_load.h"

typedef struct emulator emu_t;

emu_t *
emu_init(const char *mcu, chunk_t *chunks, uint32_t n);

int
emu_run(emu_t *emu);

void
emu_destroy(emu_t **emu);

#endif
