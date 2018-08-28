NAME = avremu

BUILD_PATH = ./build

OPT = -O2

CC = gcc
CP = objcopy
DP = objdump

CDEFS_LIST  = DEBUG
CDEFS = $(addprefix -D, $(CDEFS_LIST))

CFLAGS  = -std=gnu99 -g -Wall -I./src $(OPT) $(CDEFS)
CFLAGS += -fexceptions -pipe
CFLAGS += -Wall -Werror=format-security -Werror=implicit-function-declaration -Wno-unused

SRC  = main.c
SRC += hw/atmega328p.c
SRC += hw/devices.c
SRC += runtime/decode.c
SRC += runtime/emu.c
SRC += runtime/jit.c
SRC += util/ihex.c util/logging.c
SRC_PATH = ./src

OBJ = $(addsuffix .o, $(SRC))
OBJ_PATH = $(addprefix $(BUILD_PATH)/, $(OBJ))

DEPS = $(OBJ_PATH:%.o=%.d)

BIN = $(BUILD_PATH)/$(NAME)

TESTS_SRC = tests/decode_test.c
TESTS_OBJ = $(addsuffix .o, $(TESTS_SRC))
TESTS_DEPS = $(TESTS_OBJ:%.o=%.d)
TESTS_BIN = $(basename $(TESTS_SRC))

default: $(BIN)
	cd tests/asm && $(MAKE)

$(BIN): $(OBJ_PATH)
	$(CC) $(CFLAGS) -o $@ $(OBJ_PATH) $(LDFLAGS)


tests: $(TESTS_BIN)
	./tests/emu_test

tests/%: tests/%.c $(addprefix src/, $(SRC))
	$(CC) $(CFLAGS) -o $@ $@.c

-include $(DEPS)
$(BUILD_PATH)/%.c.o: $(SRC_PATH)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

clean:
	rm -f $(OBJ_PATH)

.PHONY: tests
