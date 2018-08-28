#include "runtime/emu.h"

#include "attributes.h"
#include "globals.h"
#include "hw/devices.h"
#include "runtime/decode.h"
#include "util/ihex.h"
#include "util/logging.h"
#include "util/bitmanip.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum emu_exception
{
	EMU_EXC_NONE = 0,
	EMU_EXC_SEGFAULT
};

typedef enum emu_exception exception_t;

typedef uint32_t cycle_t;

#define EMU_THROW_EXCEPTION( rsn, ...) \
	fprintf(stderr, \
		BAD("[" __FILE__ "]") " " \
			"Caught runtime exception while executing '%s'\n" \
			"  --> Reason: " rsn "\n" \
			"  --> File: " __FILE__ "\n" \
			"  --> Line: %d\n", \
		##__VA_ARGS__, \
		__LINE__)

struct emulator
{
	hw_t hw;
	exception_t exc;
	cycle_t cycles;
};

static void
p_set_zns(hw_t *hw, uint8_t res)
{
	SET_SREG(hw, SREG_Z, res == 0);
	SET_SREG(hw, SREG_N, res>>7);
	SET_SREG(hw, SREG_S, hw->sreg[SREG_N] ^ hw->sreg[SREG_V]);
}

static void
p_set_zns16(hw_t *hw, uint16_t res)
{
	SET_SREG(hw, SREG_Z, res == 0);
	SET_SREG(hw, SREG_N, res>>15);
	SET_SREG(hw, SREG_S, hw->sreg[SREG_N] ^ hw->sreg[SREG_V]);
}

/**
 * @brief Returns the first value off the stack and increments the stack
 * pointer.
 *
 * This function should only be called by functions that directly change the
 * state of the emulator. If the stack pointer pointed to the end of ram
 * previous to calling @p_stack_pop, then the stack pointer is set to 0.
 */
static uint8_t
p_stack_pop(hw_t *hw)
{
	uint8_t spl, sph;
	uint16_t sp = FETCH_WORD(hw->sp, 0);

	if (sp == hw->ramend)
	{
		spl = LOW(0x0000);
		sph = HIGH(0x0000);
	}
	else
	{
		++sp;
		spl = LOW(sp);
		sph = HIGH(sp);
	}

	hw->sp[0] = spl;
	hw->sp[1] = sph;

	return hw->data[sp];
}

/**
 * @brief Pushes a value onto the stack, decrementing the stack pointer and
 * returning this address.
 *
 * This function should only be called by functions that directly change the 
 * state of the emulator. If the stack pointer pointed to the beginning of data
 * memory, i.e., 0x0000, then the stack pointer is set to the last ram address
 * of the device. You can find this value under 'hw/$MYMCU.c'
 */
static uint16_t
p_stack_push(hw_t *hw)
{
	uint8_t spl, sph;
	uint16_t sp = FETCH_WORD(hw->sp, 0);

	if (sp == 0)
	{
		spl = LOW(hw->ramend);
		sph = HIGH(hw->ramend);
	}
	else
	{
		spl = LOW(sp-1);
		sph = HIGH(sp-1);
	}

	hw->sp[0] = spl;
	hw->sp[1] = sph;

	return sp;
}

static int
emu_upload(hw_t *hw, chunk_t *chunks, uint32_t n)
{
	if (!hw || !chunks || !n)
		PANIC("Bad call to %s", __FUNCTION__ );

	if (!hw->flash)
	{
		ERROR("Must call device->init before making call to %s",
			__FUNCTION__);
		return -1;
	}

	uint32_t progend = chunks[n-1].baseaddr + chunks[n-1].size - 1;
	if (progend > hw->flashend)
	{
		ERROR("Attempted to upload program beyond flash space\n");
		return -1;
	}

	uint32_t bytes = 0;

	for (size_t i = 0; i < n; i++)
	{
		chunk_t chunk = chunks[i];
		uint32_t baseaddr = chunk.baseaddr;

		memcpy(&hw->flash[baseaddr], chunk.data, chunk.size);

		bytes += chunk.size;
	}

	LOGF("Successfully uploaded %uB (%u words) to '%s'\n",
		bytes,
		bytes / 2,
		hw->name);

	return 0;
}

static void
emu_dump_core(emu_t *emu)
{
	if (!emu)
		return;

	hw_t *hw = &emu->hw;

	uint16_t sp = FETCH_WORD(hw->sp, 0);

	LOGF("Dumping core...\n"
		" --> PC: %04X (%04X)\n"
		" --> SP: %04X\n"
		" --> SREG: %u%u%u%u%u%u%u%u\n"
		" --> CYCLES: %u\n",
		hw->pc / 2, hw->pc,
		sp,
		hw->sreg[7], hw->sreg[6], hw->sreg[5], hw->sreg[4],
		hw->sreg[3], hw->sreg[2], hw->sreg[1], hw->sreg[0],
		emu->cycles);

	LOGF("Dumping data memory...\n");

	LOG("    00            07 08            0F ");
	LOG(    "10            17 18            1F ");
	LOG("             ASCII\n");
	for (uint16_t base = 0; base <= hw->ramend; base += 32)
	{
		if (base >= 0x100 && base <= 0x800)
			continue;

		if (base == 0x0820)
			LOG("    (skipping... %u words)\n", 0x800-0x100+1);

		LOG("%03X ", base);

		/* Hex Dump */
		for (uint16_t offs = 0; offs < 32; offs++)
		{
			uint16_t addr = base + offs;
			uint8_t byte = hw->data[addr];

			if (addr > hw->ramend)
				break;

			if (offs && !(offs%8))
				LOG(" ");

			//LOG("\e[104;30m%02X\e[0m", hw->data[addr]);
			if ((addr == 0x5D) || (addr == 0x5E))
				LOG("%02X", hw->data[addr]);
			else if (addr == sp)
				LOG("%02X", hw->data[addr]);
			else if (byte)
			{
				if (offs%2)
					LOG("\e[34m");
				LOG(INVERT("%02X"), hw->data[addr]);
				if (offs%2)
					LOG("\e[0m");
			}
			else
				LOG("..", hw->data[addr]);
		}

		LOG(" ");

		/* ASCII Map */
		for (uint16_t offs = 0; offs < 32; offs++)
		{
			uint16_t addr = base + offs;
			char c = hw->data[addr];

			if (!(c >= ' ' && c <= '~'))
				c = '.';

			LOG("%c", c);
		}

		LOG("\n");
	}
}

static inline void ATTR_INLINE
p_run_once(emu_t *emu, op_t op)
{
	hw_t *hw = &emu->hw;

	uint32_t next_pc = hw->pc + 2;
	cycle_t cycles = 1;

	// FIXME: Is this true?
	if (hw->pc > hw->flashend)
		next_pc = 0;

	switch (op.instr)
	{
		case ADC:
		case ADD:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t rr = hw->data[op.rr];
			uint8_t res = rd + rr;
			
			if (op.instr == ADC)
				res += hw->sreg[SREG_C];

			hw->data[op.rd] = res;

			uint8_t chk_c =  (rd&rr) | (rr&~res) | (~res&rd);

			SET_SREG(hw, SREG_C, chk_c>>7);
			SET_SREG(hw, SREG_V, ((rd & rr & ~res) | (~rd & ~rr & res )) >> 7);
			SET_SREG(hw, SREG_H, chk_c>>3);

			p_set_zns(hw, res);

			if (rd == rr)
				ASM("%s r%u\t\t; %u\n",
					(op.instr == ADC) ? "rol" : "lsl",
					op.rd,
					res);
			else if (op.instr == ADD)
				ASM("add r%u, r%u\t; %u", op.rd, op.rr, res);
			else
				ASM("adc r%u, r%u\t; %u", op.rd, op.rr, res);

			break;
		}
		case ADIW:
		{
			uint16_t curval = FETCH_WORD(hw->data, op.rd);
			uint16_t res = curval + op.k;

			hw->data[op.rd] = LOW(res);
			hw->data[op.rd+1] = HIGH(res);

			SET_SREG(hw, SREG_C, (~res & curval)>>15);
			SET_SREG(hw, SREG_V, (curval & ~res)>>15);

			p_set_zns16(hw, res);

			cycles = 2;

			ASM("adiw r%u:%u, %u\t; =%u",
				(op.rd + 1),
				op.rd,
				op.k,
				res);
			break;
		}
		case AND:
		{
			uint8_t res = hw->data[op.rd] & hw->data[op.rr];

			hw->data[op.rd] = res;

			SET_SREG(hw, SREG_V, 0);

			p_set_zns(hw, res);

			break;
		}
		case ANDI:
		{
			uint8_t res = hw->data[op.rd] & op.k;

			SET_SREG(hw, SREG_V, 0);
			p_set_zns(hw, res);

			hw->data[op.rd] = res;

			break;
		}
		case ASR:
		{
			uint8_t res = hw->data[op.rd] >> 1;

			SET_SREG(hw, SREG_C, hw->data[op.rd]);
			SET_SREG(hw, SREG_V, hw->sreg[SREG_N] ^ hw->sreg[SREG_C]);
			p_set_zns(hw, res);

			hw->data[op.rd] = res;

			break;
		}
		case BLD:
			hw->data[op.rd] |= hw->sreg[SREG_T]<<op.b;
			ASM("bld r%u, %u\n", op.rd, op.b);
			break;
		case BRBC:
		case BRCC: case BRNE: case BRPL: case BRVC:
		case BRGE: case BRHC: case BRTC: case BRID:
		case BRCS: case BREQ: case BRMI: case BRVS:
		case BRLT: case BRHS: case BRTS: case BRIE:
		{
			// xxxx xBxx xxxx xxxx
			// BRBC: B = 1
			// BRBS: B = 0
			uint8_t is_brbc = (op.raw & 0x0400)>>10;
			if (is_brbc != hw->sreg[op.s])
				next_pc += op.k*2;

			ASM("brbc %u, 0x%04X\t; %u", op.s, next_pc, op.k*2);
			break;
		}
		case BCLR:
		case CLC: case CLZ: case CLN: case CLV:
		case CLS: case CLH: case CLT: case CLI:
		case BSET:
		case SEC: case SEZ: case SEN: case SEV:
		case SES: case SEH: case SET: case SEI:
			SET_SREG(hw, op.s, (~op.raw >> 7));

			if (op.raw & 0x0080)
				ASM("bclr %u", op.s);
			else
				ASM("bset %u", op.s);

			break;
		case CBI:
		{
			uint8_t addr = IO2MEM(op.a);
			
			hw->data[addr] &= ~(1<<op.b);

			ASM("cbi 0x%02X, %u\n", addr, op.b);
			break;
		}
		case COM:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t res = 0xFF - rd;

			SET_SREG(hw, SREG_C, 1);
			SET_SREG(hw, SREG_V, 0);
			p_set_zns(hw, res);

			hw->data[op.rd] = res;

			ASM("com %u\t\t; %u", op.rd, res);
			break;
		}
		case CP:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t rr = hw->data[op.rr];
			uint8_t res = rd - rr - hw->sreg[SREG_C];

			uint8_t chk = (~rd&rr) | (rr&res) | (res&~rd);

			SET_SREG(hw, SREG_C, chk>>7);
			SET_SREG(hw, SREG_V, ((rd & ~rr & ~res) | (~rd & rr & res))>>7);
			SET_SREG(hw, SREG_H, chk>>3);
			p_set_zns(hw, res);

			ASM("cp r%u, r%u\t; %u", op.rd, op.rr, res);
			break;
		}
		case CPC:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t rr = hw->data[op.rr];
			uint8_t res = rd - rr - hw->sreg[SREG_C];

			uint8_t chk = (~rd&rr) | (rr&res) | (res&~rd);

			SET_SREG(hw, SREG_C, chk>>7);
			if (res)
				SET_SREG(hw, SREG_Z, 0);
			SET_SREG(hw, SREG_N, res>>7);
			SET_SREG(hw, SREG_V, ((rd & ~rr & ~res) | (~rd & rr & res))>>7);
			SET_SREG(hw, SREG_S, hw->sreg[SREG_N] ^ hw->sreg[SREG_V]);
			SET_SREG(hw, SREG_H, chk>>3);

			ASM("cpc r%u, r%u\t; %u", op.rd, op.rr, res);
			break;
		}
		case CPI:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t k = op.k;
			uint8_t res = rd - k;

			uint8_t chk = (~rd&k) | (k&res) | (res&~rd);

			SET_SREG(hw, SREG_C, chk>>7);
			SET_SREG(hw, SREG_V, ((rd & ~k & ~res) | (~rd & k & res))>>7);
			SET_SREG(hw, SREG_H, chk>>3);
			p_set_zns(hw, res);

			ASM("cpi r%u, %u\t; %u", op.rd, op.k, res);
			break;
		}
		case DEC:
		{
			uint8_t res = --hw->data[op.rd];

			SET_SREG(hw, SREG_V, (res+1) == 0x80);
			p_set_zns(hw, res);

			ASM("dec %u\t\t; %u", op.rd, res);
			break;
		}
		case EOR:
		{
			uint8_t res = hw->data[op.rd] ^ hw->data[op.rr];

			SET_SREG(hw, SREG_V, 0);
			p_set_zns(hw, res);

			hw->data[op.rd] = res;

			ASM("eor r%u, r%u\t; 0x%02X", op.rd, op.rr, res);
			break;
		}
		case INC:
		{
			uint8_t res = ++hw->data[op.rd];

			SET_SREG(hw, SREG_V, (res-1) == 0x7F);
			p_set_zns(hw, res);

			ASM("inc %u\t\t; %u", op.rd, res);
			break;
		}
		case LSR:
		{
			uint8_t cur = hw->data[op.rd];
			uint8_t res = cur >> 1;

			hw->data[op.rd] = res;

			SET_SREG(hw, SREG_C, cur);
			SET_SREG(hw, SREG_Z, res == 0);
			SET_SREG(hw, SREG_N, 0);
			SET_SREG(hw, SREG_V, hw->sreg[SREG_N] ^ hw->sreg[SREG_C]);
			SET_SREG(hw, SREG_S, hw->sreg[SREG_N] ^ hw->sreg[SREG_V]);

			ASM("lsr r%u\t\t; =%u (%d)", op.rd, res, res);
			break;
		}
		case FMUL:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t rr = hw->data[op.rr];

			uint16_t res = (rd * rr) << 1;

			hw->data[R0] = LOW(res);
			hw->data[R1] = HIGH(res);

			SET_SREG(hw, SREG_C, res>>15);
			SET_SREG(hw, SREG_Z, res == 0);

			ASM("fmul r%u, r%u\t; =0x%04X", op.rd, op.rr, res);
			break;
		}
		case FMULS:
		{
			int8_t rd = hw->data[op.rd];
			int8_t rr = hw->data[op.rr];

			int16_t res = (rd * rr) << 1;

			hw->data[R0] = LOW(res);
			hw->data[R1] = HIGH(res);

			SET_SREG(hw, SREG_C, res>>15);
			SET_SREG(hw, SREG_Z, res == 0);

			ASM("fmuls r%u, r%u\t; =0x%04X (%d)",
				op.rd, op.rr, res, res);
			break;
		}
		case FMULSU:
		{
			int8_t rd = hw->data[op.rd];
			uint8_t rr = hw->data[op.rr];

			int16_t res = (rd * rr) << 1;

			hw->data[R0] = LOW(res);
			hw->data[R1] = HIGH(res);

			SET_SREG(hw, SREG_C, res>>15);
			SET_SREG(hw, SREG_Z, res == 0);

			ASM("fmulsu r%u, r%u\t; =0x%04X", op.rd, op.rr, res);
			break;
		}
		case MUL:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t rr = hw->data[op.rr];

			uint16_t res = rd * rr;

			hw->data[0] = LOW(res);
			hw->data[1] = HIGH(res);

			SET_SREG(hw, SREG_C, res >> 15);
			SET_SREG(hw, SREG_Z, res == 0);

			cycles = 2;

			ASM("mul r%u, r%u\t; =%u (%d)", op.rd, op.rr, res);
			break;
		}
		case MULS:
		{
			int8_t rd = hw->data[op.rd];
			int8_t rr = hw->data[op.rr];

			int16_t res = rd * rr;

			hw->data[0] = LOW(res);
			hw->data[1] = HIGH(res);

			SET_SREG(hw, SREG_C, res >> 15);
			SET_SREG(hw, SREG_Z, res == 0);

			cycles = 2;

			ASM("mul r%u, r%u\t; =%d", op.rd, op.rr, res);
			break;
		}
		case MULSU:
		{
			int8_t rd = hw->data[op.rd];
			uint8_t rr = hw->data[op.rd];

			int16_t res = rd * rr;

			hw->data[0] = LOW(res);
			hw->data[1] = HIGH(res);

			SET_SREG(hw, SREG_C, res >> 15);
			SET_SREG(hw, SREG_Z, res == 0);

			cycles = 2;

			ASM("mulsu r%u, r%u\t; =%d", op.rd, op.rr, res);
			break;
		}
		case NEG:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t res = 0x00 - rd;

			SET_SREG(hw, SREG_C, res != 0);
			SET_SREG(hw, SREG_V, res == 0x80);
			SET_SREG(hw, SREG_H, (res | rd)>>2);
			p_set_zns(hw, res);

			hw->data[op.rd] = res;

			ASM("neg r%u\t\t; 0x%02X", rd, res);
			break;
		}
		case OR:
		{
			uint8_t res = hw->data[op.rd] | hw->data[op.rr];

			hw->data[op.rd] = res;

			SET_SREG(hw, SREG_V, 0);
			p_set_zns(hw, res);

			break;
		}
		case ORI:
		{
			uint8_t res = hw->data[op.rd] | op.k;

			hw->data[op.rd] = res;

			SET_SREG(hw, SREG_V, 0);
			p_set_zns(hw, res);

			break;
		}
		case ROR:
		{
			uint8_t curval = hw->data[op.rd];
			uint8_t res = (hw->sreg[SREG_C]<<7)|  (curval >> 1);

			hw->data[op.rd] = res;

			SET_SREG(hw, SREG_C, curval);
			SET_SREG(hw, SREG_V, hw->sreg[SREG_N] ^ hw->sreg[SREG_C]);
			p_set_zns(hw, res);

			break;
		}
		case SUB:
		case SUBI:
		{
			uint8_t rd = hw->data[op.rd];
			uint8_t rr = (op.instr == SUB) ? hw->data[op.rd] : op.k; /* TODO: rethink this */
			uint8_t res = rd - rr;

			hw->data[op.rd] = res;

			uint8_t chk_c =  (~rd&rr) | (rr&res) | (res&~rd);

			SET_SREG(hw, SREG_C, chk_c>>7);
			SET_SREG(hw, SREG_V, ((rd & ~rr & ~res) | (~rd & rr & res )) >> 7);
			SET_SREG(hw, SREG_H, chk_c>>3);
			p_set_zns(hw, res);

			ASM("%s r%u, r%u\t; %u",
				(op.instr == SUB) ? "sub" : "subi",
				op.rd, op.rr, res);
			break;
		}
		case SWAP:
		{
			uint8_t curval = hw->data[op.rd];

			hw->data[op.rd] = (curval & 0x0F)<<4 | (curval & 0xF0)>>4;

			break;
		}
		/* Input / Output */
		case BST:
		{
			uint8_t bit = hw->data[op.rd] >> op.b;

			SET_SREG(hw, SREG_T, bit);

			ASM("bst r%u, %u", op.rd, op.b);
			break;
		}
		case IN:
			hw->data[op.rd] = hw->data[IO2MEM(op.a)];

			ASM("in r%u, 0x%02X\t; %u", op.rd, IO2MEM(op.a),
				IO2MEM(op.a));
			break;
		case LD:
		{
			uint16_t addr = FETCH_WORD(hw->data, XL);
			uint8_t chk = op.raw & 3;

			if (chk == 2)
				--addr;

			hw->data[op.rd] = hw->data[addr];

			if (chk == 1)
				++addr;

			hw->data[XL] = LOW(addr);
			hw->data[XH] = HIGH(addr);

			cycles = 2;

			ASM("ld r27:26, %02X\t; %X ", addr, hw->data[addr]);
			break;
		}
		case LDD:
		case STD:
		{
			uint8_t reg = (op.raw & 0x0008) ? YL : ZL;
			uint16_t addr = op.q + FETCH_WORD(hw->data, reg);

			if (addr > hw->ramend)
			{
				EMU_THROW_EXCEPTION("Cannot %s to address 0x%0X (segmentation fault)",
					((op.instr == STD) ? "std" : "ld"),
					((op.instr == STD) ? "write" : "read"),
					addr);
				emu->exc = EMU_EXC_SEGFAULT;
			}

			if (op.instr == LDD)
			{
				hw->data[op.rd] = hw->data[addr];
				ASM("ldd r%u, %s+%u\t; =0x%04X",
					op.rd,
					((reg == YL) ? "Y": "Z"),
					op.q,
					addr);
			}
			else
			{
				hw->data[addr] = hw->data[op.rr];
				ASM("std %s+%u, r%u\t; =0x%04X",
					((reg == YL) ? "Y": "Z"),
					op.q,
					op.rr,
					addr);
			}

			cycles = 2;

			break;
		}
		case LDI:
			hw->data[op.rd] = op.k;

			ASM("ldi r%u, 0x%02X\t; %d", op.rd, op.k, op.k);
			break;
		case LPM:
		{
			uint16_t addr = FETCH_WORD(hw->data, ZL);
			uint8_t res = hw->flash[addr];

			if (op.raw & 0x0001)
			{
				++addr;

				hw->data[ZL] = LOW(addr);
				hw->data[ZH] = HIGH(addr);
			}

			hw->data[op.rd] = res;

			cycles = 3;

			ASM("lpm %04X\t; %02X", addr, res);
			break;
		}
		case MOV:
			hw->data[op.rd] = hw->data[op.rr];

			ASM("mov r%u, %u\t; =%02X",
				op.rd, op.rr, hw->data[op.rr]);
			break;
		case MOVW:
		{
			uint16_t word = FETCH_WORD(hw->data, op.rr);

			hw->data[op.rd] = LOW(word);
			hw->data[op.rd+1] = HIGH(word);

			ASM("movw r%u:%u, r%u:%u\t; 0x%02X",
				op.rd+1,
				op.rd,
				op.rr+1,
				op.rr,
				word);
			break;
		}
		case OUT:
		{
			uint8_t addr = IO2MEM(op.a);

			hw->data[addr] = hw->data[op.rr];

			ASM("out 0x%02X, r%u\t; %u", addr, op.rr, addr);
			break;
		}
		case SBI:
		{
			uint8_t addr = IO2MEM(op.a);

			hw->data[addr] |= 1<<op.b;

			ASM("sbi 0x%04X, %u", op.a, op.b);
			break;
		}

		/* Program Flow */
		case CALL:
		{
			uint16_t sp = FETCH_WORD(hw->sp, 0);

			hw->data[sp] = LOW(next_pc + 2);
			p_stack_push(hw);
			hw->data[sp-1] = HIGH(next_pc + 2);
			p_stack_push(hw);

			next_pc = op.k*2;

			cycles = 4;

			ASM("call 0x%04X", next_pc);
			break;
		}
		case CPSE:
		{
			op_t next = avr_decode(hw, next_pc);

			if (hw->data[op.rd] == hw->data[op.rr])
			{
				// Skip the appropriate number of bytes based on
				// the size of the next instruction.
				if (INSTR_IS_32(next.instr))
				{
					cycles = 3;
					next_pc += 4;
				}
				else
				{
					cycles = 2;
					next_pc += 2;
				}
			}

			ASM("cpse r%u, r%u\t; %u == %u",
				op.rd, op.rr, hw->data[op.rd], hw->data[op.rr]);
			break;
		}
		case ICALL:
		{
			uint16_t sp = FETCH_WORD(hw->sp, 0);
			uint16_t addr = FETCH_WORD(hw->data, Z) * 2;

			hw->data[sp] = LOW(next_pc);
			p_stack_push(hw);
			hw->data[sp-1] = HIGH(next_pc);
			p_stack_push(hw);

			next_pc = addr;
			cycles = 3;

			ASM("icall 0x%04X", addr);
			break;
		}
		case IJMP:
		{
			uint16_t addr = FETCH_WORD(hw->data, Z) * 2;

			next_pc = addr;
			cycles = 2;

			ASM("ijmp\t\t; 0x%04X", addr);
			break;
		}
		case JMP:
		{
			next_pc = op.k*2;
			cycles = 3;

			ASM("jmp 0x%04X", op.k*2);
			break;
		}
		case POP:
			hw->data[op.rd] = p_stack_pop(hw);

			cycles = 2;

			ASM("pop r%u\t\t; =%u", op.rd, hw->data[op.rd]);
			break;
		case PUSH:
		{
			uint8_t spl, sph;
			uint16_t sp = FETCH_WORD(hw->sp, 0);

			hw->data[sp] = hw->data[op.rr];
			p_stack_push(hw);

			cycles = 2;

			ASM("push r%u\t; =%u", op.rr, hw->data[sp]);
			break;
		}
		case RET:
		case RETI:
		{
			uint8_t pch = p_stack_pop(hw);
			uint8_t pcl = p_stack_pop(hw);

			next_pc = (pch<<8) | pcl;
			cycles = 4;

			if (op.instr == RET)
			{
				ASM("ret\t\t; 0x%04X", next_pc);
			}
			else
			{
				SET_SREG(hw, SREG_I, 1);

				ASM("reti\t\t; 0x%04X", next_pc);
			}

			break;
		}
		case RCALL:
		{
			// TODO: 22-bit
			uint16_t sp = FETCH_WORD(hw->sp, 0);

			hw->data[sp] = LOW(hw->pc + 2);
			p_stack_push(hw);
			hw->data[sp-1] = HIGH(hw->pc + 2);
			p_stack_push(hw);

			next_pc += op.k;
			cycles = 3;

			ASM("rcall\t\t; 0x%04X", next_pc);
			break;
		}
		case RJMP:
			next_pc += op.k*2;
			cycles = 2;

			ASM("rjmp\t\t; 0x%04X", next_pc);
			break;
		case SBIS:
		{
			if (hw->data[IO2MEM(op.a)] & (1<<op.b))
			{
				op_t next = avr_decode(hw, next_pc);

				if (INSTR_IS_32(next.instr))
				{
					cycles = 3;
					next_pc += 4;
				}
				else
				{
					cycles = 2;
					next_pc += 2;
				}
			}

			ASM("sbis 0x%02X, %u\n", op.a, op.b);
			break;
		}
		case SBRC:
		{
			if ((hw->data[op.rr] & (1<<op.b)) == 0)
			{
				op_t next = avr_decode(hw, next_pc);

				if (INSTR_IS_32(next.instr))
				{
					cycles = 3;
					next_pc += 4;
				}
				else
				{
					cycles = 2;
					next_pc += 2;
				}
			}

			ASM("sbrs r%u, %u", op.rr, op.b);
			break;
		}

		/* MISC */
		case SBRS:
		{
			if (hw->data[op.rr] & (1<<op.b))
			{
				op_t next = avr_decode(hw, next_pc);

				if (INSTR_IS_32(next.instr))
				{
					cycles = 3;
					next_pc += 4;
				}
				else
				{
					cycles = 2;
					next_pc += 2;
				}
			}

			ASM("sbrs r%u, %u", op.rr, op.b);
			break;
		}

		/* MISC */
		case BREAK:
			hw->state = AVR_BREAK;
			ASM("break");
		case NOP:
			ASM("nop");
			break;
		case SLEEP: // Debugging
			hw->state = AVR_SLEEP;

			ASM("sleep");
			break;
		default:
			PANIC("Caught runtime exception while executing"
				" '%s'", avr_instr_str(op.instr));
	}

	hw->pc = next_pc;
	emu->cycles += cycles;

	return;
}

/*
 * This function fails under two conditions:
 * 	1. MCU argument cannot be matched with any known device
 * 	2. @emu_upload() fails
 */
emu_t *
emu_init(char *name, chunk_t *chunks, uint32_t n)
{
	emu_t *emu = NULL;
	hw_t hw;

	size_t i, devices = N_DEVICES;

	for (i = 0; i < devices; i++)
	{
		if (!strcasecmp(name, DEVICE_LIST[i].name))
		{
			hw = DEVICE_LIST[i];
			break;
		}
	}

	if (i == devices)
	{
		ERROR("Could not match '%s' with any known device", name);
	}
	else
	{
		emu = malloc(sizeof *emu);
		if (emu)
		{
			emu->hw = hw.init();
			emu->exc = EMU_EXC_NONE;
			emu->cycles = 0;

			if (emu_upload(&emu->hw, chunks, n) == -1)
			{
				free(emu);
				emu = NULL;
			}
		}
		else
		{
			free(emu);
		}
	}

	return emu;
}

int
emu_run(emu_t *emu)
{
	int status = 0;

	if (!emu)
	{
		ERROR("Must call emu->init before making call to %s",
			__FUNCTION__);
		return -1;
	}

	LOGF("Starting emulator...\n");
	LOGF("Configured as device '%s'\n", emu->hw.name);

	hw_t *hw = &emu->hw;
	uint16_t flashend = hw->flashend;
	bool should_continue = true;

	while (should_continue)
	{
		// Simulation Step
		op_t op = avr_decode(hw, hw->pc);
		p_run_once(emu, op);

		// TODO: Remove/improve this
		if (g_debug)
			emu_dump_core(emu);

		if (emu->exc != EMU_EXC_NONE || hw->state == AVR_SLEEP)
		{
			status = -1;
			should_continue = false;

			if (g_debug == 0)
			{
				emu_dump_core(emu);
			}
		}
	}

	return status;
}

void
emu_destroy(emu_t **emu)
{
	if (!emu || !(*emu))
		return;

	(*emu)->hw.destroy(&(*emu)->hw);

	free(*emu);
	*emu = NULL;
}
