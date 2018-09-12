#include "runtime/emu.h"

#include "attributes.h"
#include "hw/devices.h"
#include "hw/data.h"
#include "hw/flash.h"
#include "runtime/decode.h"
#include "util/bitmanip.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define ASM(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

enum emu_exception
{
	EMU_EXC_NONE = 0,
	EMU_EXC_CRASH,
	EMU_EXC_SEGFAULT
};

typedef enum emu_exception exception_t;

typedef uint32_t cycle_t;

// TODO: Add proper logging utility
#ifdef COLOR_CONSOLE
	#define INVERT(str)	"\e[7m" str "\e[0m"
	#define BOLD(str)	"\e[1m" str "\e[0m"
	#define BAD(str)	"\e[31;1m" str "\e[0m"
#else
	#define INVERT(str)	str
	#define BOLD(str)	str
	#define BAD(str)	str
#endif

#define EMU_THROW_EXCEPTION( rsn, ...) \
	fprintf(stderr, \
		BAD("[" __FILE__ "]") " " \
			"Caught runtime exception while executing instruction\n" \
			"  --> Reason: " rsn "\n" \
			"  --> File: " __FILE__ "\n" \
			"  --> Line: %d\n", \
		##__VA_ARGS__, \
		__LINE__)

#define PREEMPT_SEGFAULT(hw, addr, e) \
	if (p_validate_data_address((hw), (addr), (e)) != EMU_EXC_NONE) \
	{ \
		return; \
	}

struct emulator
{
	hw_t *hw;
	exception_t exc;
	cycle_t cycles;
};

static void
p_set_zns(hw_t *hw, uint8_t res)
{
	hw->sreg.z = res == 0;
	hw->sreg.n = res >> 7;
	hw->sreg.s = hw->sreg.n ^ hw->sreg.v;
}

static void
p_set_zns16(hw_t *hw, uint8_t res)
{
	hw->sreg.z = res == 0;
	hw->sreg.n = res >> 15;
	hw->sreg.s = hw->sreg.n ^ hw->sreg.v;
}

static uint32_t
p_stack_push(hw_t *hw, uint8_t byte)
{
	uint16_t next;
	uint16_t prev = (hw->sp[1] << 8) | hw->sp[0];

	if (prev == 0)
	{
		next = hw->ramend;
		printf("warning: push() wrapping stack pointer\n");
	}
	else
	{
		next = prev - 1;
	}

	hw->sp[0] = LOW(next);
	hw->sp[1] = HIGH(next);

	data_write(hw->data, prev, byte);

	return prev;
}

static uint8_t
p_stack_pop(hw_t *hw)
{
	uint16_t next;
	uint16_t prev = (hw->sp[1] << 8) | hw->sp[0];

	if (prev == hw->ramend)
	{
		next = 0;
		printf("warning: pop() wrapping stack pointer\n");
	}
	else
	{
		next = prev + 1;
	}

	hw->sp[0] = LOW(next);
	hw->sp[1] = HIGH(next);

	return data_read(hw->data, next);
}

/* Instead of adding a segfault passback to data.c/flash.c, all read/writes will
 * be proxied through this function. This is because only a few instructions
 * (ones which load an address) can segfault.
 */

static inline exception_t ATTR_INLINE
p_validate_data_address(hw_t *hw, uint32_t addr, exception_t *throw)
{
	if (addr > hw->ramend)
	{
		*throw = EMU_EXC_SEGFAULT;
		EMU_THROW_EXCEPTION("Cannot read from address 0x%08X "
				"(segmentation fault)", addr);
	}

	return *throw;
}

static inline void ATTR_INLINE
p_run_once(emu_t *emu, op_t op)
{
	hw_t *hw = emu->hw;

	uint32_t next_pc = hw->pc + 1;
	cycle_t cycles = 1;

	// FIXME: Is this true?
	if (hw->pc > hw->flashend)
		next_pc = 0;

	switch (op.instr)
	{
		case UNDEF:
			emu->exc = EMU_EXC_CRASH;
			break;

		/* ARITHMETIC */
		case ADD: case LSL: // ADD ==> LSL
		case ADC: case ROL: // ADC ==> ROL
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = data_read(hw->data, op.rr);
			uint8_t res = rd + rr;

			if (op.instr == ADC)
				res += hw->sreg.c;

			data_write(hw->data, op.rd, res);

			{
				uint8_t chk_c = (rd & rr) | (rr & ~res) | (~res & rd);

				hw->sreg.c = chk_c >> 7;
				hw->sreg.v = ((rd & rr & ~res) | (~rd & ~rr & res)) >> 7;
				hw->sreg.h = chk_c >> 3;

				p_set_zns(hw, res);
			}

			if (rd != rr)
			{
				ASM("%s r%u, r%u\t; %u", (op.instr == ADC) ? "adc" : "add",
						op.rd, op.rr, res);
			}
			else
			{
				ASM("%s r%u\t\t; %u", (op.instr == ADC) ? "rol" : "lsl",
						op.rd, res);
			}

			break;
		}
		case ADIW:
		case SBIW:
		{
			uint16_t cur = data_read_word(hw->data, op.rd);
			uint16_t res = cur;
			
			if (op.instr == ADIW)
				res += op.k;
			else
				res -= op.k;

			data_write_word(hw->data, op.rd, res);

			{
				hw->sreg.c = (~res & cur)>>15;
				hw->sreg.v = (cur & ~res)>>15;
				p_set_zns16(hw, res);
			}

			cycles = 2;

			ASM("%s r%u:%u, %u\t; =%u", (op.instr == ADIW) ? "adiw" : "sbiw",
					op.rd + 1, op.rd, op.k, res);
			break;
		}
		case SUB:
		case SUBI:
		case SBC:
		case SBCI:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = (op.instr == SUB || op.instr == SBC) ?
					data_read(hw->data, op.rr) : op.k;

			uint8_t res = rd - rr;

			if (op.instr == SBC || op.instr == SBCI)
				res -= hw->sreg.c;

			data_write(hw->data, op.rd, res);

			{
				uint8_t chk_c = (~rd & rr) | (rr & res) | (res & ~rd);

				hw->sreg.c = chk_c >> 7;
				hw->sreg.v = ((rd & ~rr & ~res) | (~rd & rr & res)) >> 7;
				hw->sreg.h = chk_c >> 3;

				p_set_zns(hw, res);
			}

			break;
		}
		case DEC:
		{
			uint8_t res = data_read(hw->data, op.rd) - 1;

			data_write(hw->data, op.rd, res);

			hw->sreg.v = ((res+1) == 0x80);
			p_set_zns(hw, res);

			ASM("dec r%u\t\t; %u", op.rd, res);
			break;
		}
		case INC:
		{
			uint8_t res = data_read(hw->data, op.rd) + 1;

			data_write(hw->data, op.rd, res);

			hw->sreg.v = ((res-1) == 0x7F);
			p_set_zns(hw, res);

			ASM("inc r%u\t\t; %u", op.rd, res);
			break;
		}
		case MUL:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = data_read(hw->data, op.rr);

			uint16_t res = rd * rr;

			data_write_word(hw->data, 0, res);

			hw->sreg.c = res >> 15;
			hw->sreg.z = res == 0;

			cycles = 2;

			ASM("mul r%u, r%u\t; =%u", op.rd, op.rr, res);
			break;
		}
		case MULS:
		{
			int8_t rd = data_read(hw->data, op.rd);
			int8_t rr = data_read(hw->data, op.rr);

			int16_t res = rd * rr;

			data_write_word(hw->data, 0, res);

			hw->sreg.c = res >> 15;
			hw->sreg.z = res == 0;

			cycles = 2;

			ASM("muls r%u, r%u\t; =%d", op.rd, op.rr, res);
			break;
		}
		case MULSU:
		{
			int8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = data_read(hw->data, op.rr);

			int16_t res = rd * rr;

			data_write_word(hw->data, 0, res);

			hw->sreg.c = res >> 15;
			hw->sreg.z = res == 0;

			cycles = 2;

			ASM("mulsu r%u, r%u\t; =%d", op.rd, op.rr, res);
			break;
		}
		case FMUL:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = data_read(hw->data, op.rr);

			uint16_t res = (rd * rr) << 1;

			// TODO: 0 => R0
			data_write_word(hw->data, 0, res);

			hw->sreg.c = res >> 15;
			hw->sreg.z = res == 0;

			ASM("fmul r%u, r%u\t; =0x%04X", op.rd, op.rr, res);
			break;
		}
		case FMULS:
		{
			int8_t rd = data_read(hw->data, op.rd);
			int8_t rr = data_read(hw->data, op.rr);

			int16_t res = (rd * rr) << 1;

			data_write_word(hw->data, 0, res);

			hw->sreg.c = res >> 15;
			hw->sreg.z = res == 0;

			ASM("fmuls r%u, r%u\t; =0x%04X", op.rd, op.rr, res);
			break;
		}
		case FMULSU:
		{
			int8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = data_read(hw->data, op.rr);

			int16_t res = (rd * rr) << 1;

			data_write_word(hw->data, 0, res);

			hw->sreg.c = res >> 15;
			hw->sreg.z = res == 0;

			ASM("fmulsu r%u, r%u\t; =0x%04X", op.rd, op.rr, res);
			break;
		}

		/* LOGIC */
		case AND: case TST:
		case ANDI: case CBR:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = (op.instr == AND ) ? data_read(hw->data, op.rr) : op.k;

			uint8_t res = rd & rr;

			hw->sreg.v = 0;
			p_set_zns(hw, res);

			ASM("%s r%u, %s%u\t =%X",
					(op.instr == OR) ? "and" : "andi", op.rd,
					(op.instr == OR) ? "r"   : ""    , op.rr,
					res);
			break;
		}
		case EOR: case CLR:
		{
			uint8_t res = data_read(hw->data, op.rd) ^ data_read(hw->data, op.rr);

			data_write(hw->data, op.rd, res);

			hw->sreg.v = 0;
			p_set_zns(hw, res);

			ASM("eor r%u, r%u", op.rd, op.rr);
			break;
		}
		case COM:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t res = 0xFF - rd;

			data_write(hw->data, op.rd, res);

			hw->sreg.c = 1;
			hw->sreg.v = 0;
			p_set_zns(hw, res);

			ASM("com r%u\t\t =%X", op.rd, res);
			break;
		}
		case NEG:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t res = 0x00 - rd;

			data_write(hw->data, op.rd, res);

			hw->sreg.c = res != 0;
			hw->sreg.v = res == 0x80;
			hw->sreg.h = (res | rd) >> 2;
			p_set_zns(hw, res);

			ASM("neg r%u\t\t =%X", op.rd, res);
			break;
		}
		case OR:
		case ORI: case SBR:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = (op.instr == OR) ? data_read(hw->data, op.rr) : op.k;

			uint8_t res = rd | rr;

			data_write(hw->data, op.rd, res);

			hw->sreg.v = 0;
			p_set_zns(hw, res);

			ASM("%s r%u, %s%u\t =%X",
					(op.instr == OR) ? "or" : "ori", op.rd,
					(op.instr == OR) ? "r"  : ""   , op.rr,
					res);
			break;
		}

		/* BRANCH */
		case CALL: // TODO: Write asm unit test for relocatable call
		{
			// CALL is 32-bit so increment pc again
			p_stack_push(hw, LOW(next_pc + 1));
			p_stack_push(hw, HIGH(next_pc + 1));

			next_pc = op.k;

			cycles = 4;

			ASM("call 0x%08x", next_pc);
			break;
		}
		case ICALL:
		{
			uint16_t addr = data_read_word(hw->data, 30); // TODO

			p_stack_push(hw, LOW(next_pc + 1));
			p_stack_push(hw, HIGH(next_pc + 1));

			next_pc = addr;

			cycles = 3;

			ASM("icall 0x%08X", addr);
			break;
		}
		case RCALL:
		{
			p_stack_push(hw, LOW(next_pc + 1));
			p_stack_push(hw, HIGH(next_pc + 1));

			next_pc += op.k;

			cycles = 3;

			ASM("rcall %X %X", op.k, op.raw);
			break;
		}
		/* TODO: EICALL (not on 328) */
		case JMP:
		{
			next_pc = op.k;
			cycles = 3;

			ASM("jmp .+0x%04X", op.k);
			break;
		}
		case IJMP:
		{
			uint16_t addr = data_read_word(hw->data, Z);

			next_pc = addr;
			cycles = 2;

			ASM("ijmp .+0x%04X", addr);
			break;
		}
		case RJMP:
		{
			next_pc += op.k;
			cycles = 2;

			ASM("rjmp 0x%04X", next_pc);
			break;
		}
		case RET:
		case RETI:
		{
			uint8_t pch = p_stack_pop(hw);
			uint8_t pcl = p_stack_pop(hw);

			printf("HIGH = %X\n", pch);

			next_pc = pcl | (pch<<8);
			cycles = 4;

			if (op.instr == RETI)
				hw->sreg.i = 1;

			ASM("%s\t\t; 0x%04X", avr_op_str(op.instr), next_pc);
		}
		/* TODO: EIJMP */
		case CP:
		case CPI:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = (op.instr == CP) ? data_read(hw->data, op.rr) : op.k;
			uint8_t res = rd - rr;

			{
				uint8_t chk = (~rd & rr) | (rr & res) | (res & ~rd);
				hw->sreg.c = chk>>7;
				hw->sreg.v = ((rd & ~rr & ~res) | (~rd & rr & res)) >> 7;
				hw->sreg.h = chk>>3;
				p_set_zns(hw, res);
			}

			ASM("cp%s r%u, r%u\t; %u", (op.instr == CP) ? "" : "i",
					op.rd, op.rr, res);
			break;
		}
		case CPC:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = data_read(hw->data, op.rr);
			uint8_t res = rd - rr - hw->sreg.c;

			{
				uint8_t chk = (~rd & rr) | (rr & res) | (res & ~rd);
				hw->sreg.c = chk >> 7;
				if (res)
					hw->sreg.z = 0;
				hw->sreg.n = res>>7;
				hw->sreg.v = ((rd & ~rr & ~res) | (~rd & rr & res)) >> 7;
				hw->sreg.s = hw->sreg.n ^ hw->sreg.v;
				hw->sreg.h = chk >> 3;
			}

			ASM("cpc r%u, r%u\t =%u", op.rd, op.rr, res);
			break;
		}
		case CPSE:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t rr = data_read(hw->data, op.rr);

			if (rd == rr)
			{
				op_t next = avr_decode(hw, next_pc);

				if (INSTR_IS_32(next.instr))
				{
					cycles = 3;
					next_pc += 2;
				}
				else
				{
					cycles = 2;
					next_pc += 1;
				}
			}

			ASM("cpse r%u, r%u\t; %u == %u", op.rd, op.rr, rd, rr);
			break;
		}
		case SBRC: /* Register File */
		case SBIC: /* I/O Register */
		{
			uint8_t rr = (op.instr == SBRC) ?
				data_read(hw->data, op.rr) : data_read(hw->data, IO2MEM(op.a));

			if ((rr & (1<<op.b)) == 0)
			{
				op_t next = avr_decode(hw, next_pc);

				if (INSTR_IS_32(next.instr))
				{
					cycles = 3;
					next_pc += 2;
				}
				else
				{
					cycles = 2;
					next_pc += 1;
				}
			}

			ASM("sbrc r%u, r%u", op.rr, op.b);
			break;
		}
		case SBRS: /* Register File */
		case SBIS: /* I/O Register */
		{
			uint8_t rr = (op.instr == SBRS) ?
				data_read(hw->data, op.rr) : data_read(hw->data, IO2MEM(op.a));

			if ((rr & (1<<op.b)) == 1)
			{
				op_t next = avr_decode(hw, next_pc);

				if (INSTR_IS_32(next.instr))
				{
					cycles = 3;
					next_pc += 2;
				}
				else
				{
					cycles = 2;
					next_pc += 1;
				}
			}

			ASM("sbrs r%u, r%u", op.rr, op.b);
			break;
		}
		case BRBC:
		case BRSH: /* BRSH => BRCC */
		case BRCC: case BRNE: case BRPL: case BRVC:
		case BRGE: case BRHC: case BRTC: case BRID:
		case BRBS:
		case BRLO: /* BRLO => BRCS */
		case BRCS: case BREQ: case BRMI: case BRVS:
		case BRLT: case BRHS: case BRTS: case BRIE:
		{
			uint8_t is_brbc = (op.raw & 0x0400)>>10;
			uint8_t sbit = 0;

			switch (op.s)
			{
				case 0: sbit = hw->sreg.c; break;
				case 1: sbit = hw->sreg.z; break;
				case 2: sbit = hw->sreg.n; break;
				case 3: sbit = hw->sreg.v; break;
				case 4: sbit = hw->sreg.s; break;
				case 5: sbit = hw->sreg.h; break;
				case 6: sbit = hw->sreg.t; break;
				case 7: sbit = hw->sreg.i; break;
			}

			if (sbit != is_brbc)
				next_pc += op.k;

			ASM("brbs sreg[%u], . + 0x%04X\t =0x%04X", op.s, op.k, next_pc);
			break;
		}

		/* Bit Manipulation */
		case ASR:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t res = rd >> 1;

			hw->sreg.c = rd;
			hw->sreg.v = hw->sreg.n ^ hw->sreg.c;
			p_set_zns(hw, res);

			data_write(hw->data, op.rd, res);

			ASM("asr r%u\t\t =%X (%d)", op.rd, res, res);
			break;
		}
		case LSR:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t res = rd >> 1;

			data_write(hw->data, op.rd, res);

			hw->sreg.c = rd;
			hw->sreg.z = res == 0;
			hw->sreg.n = 0;
			hw->sreg.v = hw->sreg.n ^ hw->sreg.c;
			hw->sreg.s = hw->sreg.n ^ hw->sreg.v;

			ASM("lsr r%u\t\t =%X (%d)", op.rd, res, res);
			break;
		}
		case ROR:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t res = (hw->sreg.c << 7) | (rd >> 1);

			data_write(hw->data, op.rd, res);

			hw->sreg.c = rd;
			hw->sreg.v = hw->sreg.n ^ hw->sreg.c;
			p_set_zns(hw, res);

			ASM("ror r%u\t\t =%X (%d)", op.rd, res, res);
			break;
		}
		case SWAP:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t res = (rd & 0x0F)<<4 | (rd & 0xF0)>>4;

			data_write(hw->data, op.rd, res);

			ASM("swap r%u\t\t = %X", op.rd, res);
			break;
		}
		case SBI:
		case CBI:
		{
			uint8_t addr = IO2MEM(op.a);
			uint8_t res = data_read(hw->data, addr);
			
			if (op.instr == SBI)
				res |= (1<<op.b);
			else
				res &= ~(1<<op.b);

			data_write(hw->data, addr, res);

			ASM("%s 0x%04X, %u\t =%u", avr_op_str(op.instr), addr, op.b, res);
			break;
		}
		case BSET:
		case SEC: case SEZ: case SEN: case SEV:
		case SES: case SEH: case SET: case SEI:
		case BCLR:
		case CLC: case CLZ: case CLN: case CLV:
		case CLS: case CLH: case CLT: case CLI:
		{
			// BCLR: xxxx xxxx 1sss xxxx
			// BSET: xxxx xxxx 0sss xxxx
			uint8_t sbit = ~(op.raw >> 7);

			switch (op.s)
			{
				case 0: hw->sreg.c = sbit; break;
				case 1: hw->sreg.z = sbit; break;
				case 2: hw->sreg.n = sbit; break;
				case 3: hw->sreg.v = sbit; break;
				case 4: hw->sreg.s = sbit; break;
				case 5: hw->sreg.h = sbit; break;
				case 6: hw->sreg.t = sbit; break;
				case 7: hw->sreg.i = sbit; break;
			}

			ASM("%s sreg[%u]", (op.raw & 0x0080) ? "bclr" : "bset", op.s);
			break;
		}
		case BLD:
		{
			uint8_t rd = data_read(hw->data, op.rd);
			uint8_t res = rd | (hw->sreg.t << op.b);

			data_write(hw->data, op.rd, res);

			ASM("bld r%u, %u\t =%X", op.rd, op.b, res);
			break;
		}
		case BST:
		{
			uint8_t bit = data_read(hw->data, op.rd) >> op.b;

			hw->sreg.t = bit;

			ASM("bst r%u, %u\t =%X", op.rd, op.b, bit);
			break;
		}

		/* Data Transfer */
		case IN:
		{
			uint8_t val = data_read(hw->data, IO2MEM(op.a));

			data_write(hw->data, op.rd, val);

			ASM("in r%u, 0x%02X\t; =%X", op.rd, IO2MEM(op.a), val);
			break;
		}
		case OUT:
		{
			uint8_t val = data_read(hw->data, op.rr);

			data_write(hw->data, IO2MEM(op.a), val);

			ASM("out 0x%02X, r%u\t; =%X", IO2MEM(op.a), op.rr, val);
			break;
		}
		case LD:
		case ST:
		{
			uint16_t addr = data_read_word(hw->data, X);

			PREEMPT_SEGFAULT(hw, addr, &emu->exc);

			uint8_t adj = op.raw & 3;
			if (adj == 2)
				--addr;

			if (op.instr == LD)
			{
				uint8_t val = data_read(hw->data, addr);
				data_write(hw->data, op.rd, val);
				ASM("ld r%u, %X\t =%X", op.rd, addr, val);
			}
			else
			{
				uint8_t val = data_read(hw->data, op.rr);
				data_write(hw->data, addr, val);
				ASM("st %X, r%u\t =%X", addr, op.rr, val);
			}

			if (adj == 1)
				++addr;

			data_write_word(hw->data, X, addr);
			
			cycles = 2;

			break;
		}
		case LDD:
		case STD:
		{
			uint8_t reg = (op.raw & 0x0008) ? Y : Z;
			uint16_t addr = op.q + data_read_word(hw->data, reg);

			PREEMPT_SEGFAULT(hw, addr, &emu->exc);

			if (op.instr == LDD)
			{
				uint8_t val = data_read(hw->data, addr);
				data_write(hw->data, op.rd, val);
				ASM("ldd r%u, %X\t; =%X", op.rd, addr, val);
			}
			else /* STD */
			{
				uint8_t val = data_read(hw->data, op.rr);
				data_write(hw->data, addr, val);
				ASM("std %X, r%u\t; =%X", addr, op.rr, val);
			}

			break;
		}
		case LDI:
		{
			data_write(hw->data, op.rd, op.k);

			ASM("ldi r%u, 0x%02X\t; %d", op.rd, op.k, op.k);
			break;
		}
		/* ATtiny and few others only/
		case LDS:
		case STS:
		{
			PREEMPT_SEGFAULT(hw, op.k, &emu->exc);

			if (op.instr == LDS)
			{
				uint8_t val = data_read(hw->data, op.k);
				data_write(hw->data, op.rd, val);
				ASM("ldd r%u, %X\t; =%X", op.rd, op.k, val);
			}
			else STS
			{
				uint8_t val = data_read(hw->data, op.rr);
				data_write(hw->data, op.k, op.rr);
				ASM("sts %X, r%u\t; =%X", op.k, op.rr, val);
			}

			break;
		} */
		/* case LPM: */
		/* case SPM: */
		case MOV:
		{
			uint8_t rr = data_read(hw->data, op.rr);
			data_write(hw->data, op.rd, rr);

			ASM("mov r%u, r%u\t; =0x%02X", op.rd, op.rr, rr);
			break;
		}
		case MOVW:
		{
			uint16_t res16 = data_read(hw->data, op.rr);
			data_write_word(hw->data, op.rd, res16);

			ASM("movw r%u:%u, r%u:%u\t; =0x%02X",
					op.rd, op.rd+1, op.rr, op.rr+1, res16);
			break;
		}
		case PUSH:
		{
			uint8_t rr = data_read(hw->data, op.rr);
			p_stack_push(hw, rr);

			cycles = 2;

			ASM("push r%u\t; =%X", op.rr, rr);
			break;
		}
		case POP:
		{
			uint8_t val = p_stack_pop(hw);
			data_write(hw->data, op.rd, val);

			cycles = 2;

			ASM("pop r%u\t; =%X", op.rd, val);
			break;
		}

		/* MCU Control */
		case BREAK:
		{
			hw->state = AVR_BREAK;
			ASM("break");
			break;
		}
		case NOP:
		{
			ASM("nop");
			break;
		}
		case SLEEP:
		{
			hw->state = AVR_SLEEP;
			ASM("sleep");
			break;
		}
		case WDR:
		case DES:
		case XCH:
		{
			/* TODO */
			ASM("todo");
			break;
		}

		default:
			break;
	}

	hw->pc = next_pc;
	emu->cycles += cycles;

	return;
}

emu_t *
emu_init(const char *mcu, chunk_t *chunks, uint32_t n)
{
	emu_t *emu = malloc(sizeof *emu);
	hw_t *hw = device_by_name(mcu);

	if (hw == NULL)
	{
		// TODO: Couldn't match
		free(emu);
	}

	if (emu)
	{
		emu->hw = hw;
		emu->exc = EMU_EXC_NONE;
		emu->cycles = 0;

		int status = flash_upload(hw->flash, chunks, n);
		if (status == -1)
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

	hw_t *hw = emu->hw;
	bool should_continue = true;

	while (should_continue)
	{
		op_t op = avr_decode(hw, hw->pc);

		p_run_once(emu, op);
		printf("PC %X\n", hw->pc);
		printf("SP %X%X\n", hw->sp[1], hw->sp[0]);
		printf("SREG %u%u%u%u %u%u%u%u\n",
				hw->sreg.i,
				hw->sreg.t,
				hw->sreg.h,
				hw->sreg.s,
				hw->sreg.v,
				hw->sreg.n,
				hw->sreg.s,
				hw->sreg.z);
		data_dump(hw->data, 0, 32);
		data_dump(hw->data, 0x800, 0x8FF);

		if (emu->exc != EMU_EXC_NONE || hw->state == AVR_BREAK)
		{
			status = -1;
			should_continue = false;
		}

		while (getchar() != '\n');
	}

	return status; // TODO
}

void
emu_destroy(emu_t **emu)
{
	emu_t *_emu = *emu;

	if (_emu)
	{
		_emu->hw->destroy(&_emu->hw);
		free(_emu);
		*emu = NULL;
	}
}
