#ifndef RHEA_EMU_H
#define RHEA_EMU_H

#include "util/ihex.h"

struct emulator;
typedef struct emulator emu_t;

emu_t *
emu_init(char *mcu, chunk_t *chunks, uint32_t n);

int
emu_run(emu_t *emu);

void
emu_destroy(emu_t **emu);

#endif
