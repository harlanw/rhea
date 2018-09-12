DEBUG=1

RHEA_BUILD_PATH = build
RHEA_BUILD_OPTS = _POSIX_C_SOURCE=200809L USE_MEMTRACK CONSOLE_COLOR
RHEA_SRC_PATH = rhea

RHEA = $(RHEA_BUILD_PATH)/rhea

CC = gcc
CDEFS = $(addprefix -D, $(RHEA_BUILD_OPTS))

CFLAGS = -std=c99 -I./rhea $(CDEFS) \
         -pipe \
         -fpack-struct -fshort-enums -funsigned-char -funsigned-bitfields \
         -Wall -Wpedantic -Werror=format-security \
         -Werror=implicit-function-declaration \
         -Wno-unused -Wpedantic

ifeq ($(DEBUG), 1)
	CFLAGS += -g -DDEBUG
endif

SRC = rhea.c \
      rhea_args.c rhea_load.c rhea_utils.c \
      rhea_ihex.c \
      hw/data.c  hw/flash.c \
      hw/devices.c hw/atmega328p.c \
      runtime/emu.c runtime/decode.c

OBJ = $(addprefix $(RHEA_BUILD_PATH)/, $(addsuffix .o, $(SRC)))

DEPS = $(OBJ:%.o=%.d)

default: $(RHEA)
	cd tests/asm && $(MAKE)

$(RHEA): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

-include $(DEPS)
$(RHEA_BUILD_PATH)/%.c.o: $(RHEA_SRC_PATH)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

clean:
	rm -rf $(RHEA_BUILD_PATH)

.PHONY: clean default
