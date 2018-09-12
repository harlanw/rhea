#include "runtime/decode.h"

#include "attributes.h"

#include "hw/flash.h"

#include <stdio.h>
#include <stdlib.h>

#define IN_RANGE(var,min,max) (((var) >= (min)) && ((var) <= (max)))
#define NIBBLE(i,n) (((i) & (0xF<<((n)*4)))>>((n)*4))

#define GET_R5(reg, raw) \
{ \
	reg = (raw & 0x01F0) >>4; \
}

#define GET_D4K8(op, raw) \
{ \
	op.rd = 16 + ((raw & 0x00F0)>>4); \
	op.k = ((raw & 0x0F00)>>4) | (raw & 0x000F); \
}

#define GET_D5R5(op, raw) \
{ \
	op.rd = (raw & 0x01F0)>>4; \
	op.rr = (raw & 0x0200)>>5 | (raw & 0x000F); \
}

const char *
avr_op_str(instr_t instr)
{
	static const char *INSTR_STR_LUT[] = 
	{
		"undef",

		/* ARITHMETIC */
		"add", "adc", "adiw",
		"lsl",
		"rol",
		"sub", "subi", "sbc", "sbci", "sbiw",
		"dec", "inc",
		"mul", "muls", "mulsu",
		"fmul", "fmuls", "fmulsu",

		/* LOGIC */
		"and", "andi", "cbr",
		"tst",
		"eor",
		"clr",
		"com", "neg",
		"or", "ori",
		"sbr",
		"ser",

		/* BRANCH */
		"call", "icall", "rcall", "eicall",
		"jmp", "ijmp", "rjmp", "eijmp",
		"ret", "reti",

		"cp", "cpi", "cpc", "cpse",

		"sbrc", "sbrs",
		"sbic", "sbis",

		"brbs",
		"brcs", "breq", "brmi", "brvs", "brlt", "brhs", "brts", "brie",
		"brlo",

		"brbc",
		"brcc", "brne", "brpl", "brvc", "brge", "brhc", "brtc", "brid",
		"brsh",

		/* Bit, Bit-Test */
		"asr", "lsr", "ror",
		"swap",
		"sbi", "cbi",

		"bset",
		"sec", "sez", "sen", "sev", "ses", "seh", "set", "sei",
		"bclr",
		"clc", "clz", "cln", "clv", "cls", "clh", "clt", "cli",

		"bld", "bst",

		/* Data Transfer */
		"in", "out",
		"ld", "ldd", "ldi", "lds",
		"st", "std", "sts",
		"lpm", "spm", "elpm",
		"mov", "movw",
		"push", "pop",

		/* MCU Control */
		"break", "nop", "sleep", "wdr",

		/* MISC. */
		"des", "xch"
	};

	if (instr > XCH)
	{
		instr = UNDEF;
	}

	return INSTR_STR_LUT[instr];
}

static inline instr_t ATTR_INLINE
p_decode_row00(uint16_t raw)
{
	instr_t instr = UNDEF;

	switch (raw & 0xFF00)
	{
		case 0x0000:
		{
			if (!raw)
				instr = NOP;

			break;
		}
		case 0x0100: instr = MOVW; break;
		case 0x0200: instr = MULS; break;
		case 0x0300:
		{
			if (raw & 0x0080)
			{
				if (raw & 0x0008)
					instr = FMULSU;
				else
					instr = FMULS;
			}
			else
			{
				if (raw & 0x0008)
					instr = FMUL;
				else
					instr = MULSU;
			}

			break;
		}
		case 0x0400:
		case 0x0500:
		case 0x0600:
		case 0x0700:
			instr = CPC;
			break;
		case 0x0800:
		case 0x0900:
		case 0x0A00:
		case 0x0B00:
			instr = SBC;
			break;
		case 0x0C00:
		case 0x0D00:
		case 0x0E00:
		case 0x0F00:
			instr = ADD;
			break;
	}

	return instr;
}

static inline instr_t ATTR_INLINE
p_decode_row01(uint16_t raw)
{
	instr_t instr = UNDEF;

	switch (raw & 0xFC00)
	{
		case 0x1000: instr = CPSE; break;
		case 0x1400: instr = CP; break;
		case 0x1800: instr = SUB; break;
		case 0x1C00: instr = ADC; break;
	}

	return instr;
}

static inline instr_t ATTR_INLINE
p_decode_row02(uint16_t raw)
{
	instr_t instr = UNDEF;

	switch (raw & 0xFC00)
	{
		case 0x2000: instr = AND; break;
		case 0x2400: instr = EOR; break;
		case 0x2800: instr = OR; break;
		case 0x2C00: instr = MOV; break;
	}

	return instr;
}

static inline instr_t ATTR_INLINE
p_decode_load_store(uint16_t raw)
{
	instr_t instr = UNDEF;

	if (raw & 0x0200)
		instr = STD;
	else
		instr = LDD;

	return instr;
}

static inline instr_t ATTR_INLINE
p_decode_row09(uint16_t oraw)
{
	instr_t instr = UNDEF;

	uint8_t idx0 = NIBBLE(oraw, 0);
	uint8_t idx1 = NIBBLE(oraw, 1);
	uint8_t idx2 = NIBBLE(oraw, 2);

	switch (NIBBLE(oraw, 2))
	{
		case 0: case 1:
			switch (idx0)
			{
				case 0:
					instr = LDS;
					break;
				case 1: case 2:
				case 9: case 10:
				case 12: case 13: case 14:
					instr = LD;
					break;
				case 4: case 5:
					instr = LPM;
					break;
				case 6: case 7:
					instr = ELPM;
					break;
				case 15:
					instr = POP;
					break;
			}
			break;
		case 2: case 3:
			switch (idx0)
			{
				case 0:
					instr = STS;
					break;
				case 1: case 2:
				case 9: case 10:
				case 12: case 13: case 14:
					instr = ST;
					break;
				case 15:
					instr = PUSH;
					break;
			}
			break;
		case 4: case 5:
			switch (idx0)
			{
				case 0:
					instr = COM;
					break;
				case 1:
					instr = NEG;
					break;
				case 2:
					instr = SWAP;
					break;
				case 3:
					instr = INC;
					break;
				case 5:
					instr = ASR;
					break;
				case 6:
					instr = LSR;
					break;
				case 7:
					instr = ROR;
					break;
				case 8:
					if (idx2 == 4)
					{
						switch (idx1)
						{
							case 0: instr = SEC; break;
							case 1: instr = SEZ; break;
							case 2: instr = SEN; break;
							case 3: instr = SEV; break;
							case 4: instr = SES; break;
							case 5: instr = SEH; break;
							case 6: instr = SET; break;
							case 7: instr = SEI; break;
							case 8: instr = CLC; break;
							case 9: instr = CLZ; break;
							case 10: instr = CLN; break;
							case 11: instr = CLV; break;
							case 12: instr = CLS; break;
							case 13: instr = CLH; break;
							case 14: instr = CLT; break;
							case 15: instr = CLI; break;
						}
					}
					else
					{
						switch (idx1)
						{
							case 0: instr = RET; break;
							case 1: instr = RETI; break;
							case 8: instr = SLEEP; break;
							case 9: instr = BREAK; break;
							case 10: instr = WDR; break;
							case 12: instr = LPM; break;
							case 13: instr = ELPM; break;
							case 14: instr = SPM; break;
							case 15: instr = SPM; break;
						}
					}
					break;
				case 9:
					if (idx2 == 4)
					{
						if (idx1 == 0)
							instr = IJMP;
						else if (idx1 == 1)
							instr = EIJMP;
					}
					else
					{
						if (idx1 == 0)
							instr = ICALL;
						else if (idx1 == 1)
							instr = EICALL;
					}
					break;
				case 10:
					instr = DEC;
					break;
				case 11:
					if (idx2 == 4)
						instr = DES;
					break;
				case 12: case 13:
					instr = JMP;
					break;
				case 14: case 15:
					instr = CALL;
					break;
			}
			break;
		case 6:
			instr = ADIW;
			break;
		case 7:
			instr = SBIW;
			break;
		case 8:
			instr = CBI;
			break;
		case 9:
			instr = SBIC;
			break;
		case 10:
			instr = SBI;
			break;
		case 11:
			instr = SBIS;
			break;
		case 12: case 13: case 14: case 15:
			instr = MUL;
	}

	return instr;
}

static inline instr_t ATTR_INLINE
p_decode_in_out(uint16_t raw)
{
	instr_t instr = UNDEF;

	if (raw & 0x0800)
		instr = OUT;
	else
		instr = IN;

	return instr;
}

static inline instr_t ATTR_INLINE
p_decode_branch(uint16_t raw)
{
	instr_t instr = UNDEF;

	uint8_t idx0 = NIBBLE(raw, 0);
	uint8_t idx2 = NIBBLE(raw, 2);

	switch (raw & 0xFE00)
	{
		case 0xF000:
		case 0xF200:
		case 0xF400:
		case 0xF600:
		{
			static const instr_t SREG_BRANCH_LUT[2][8] =
			{
				{ BRCS, BREQ, BRMI, BRVS, BRLT, BRHS, BRTS, BRIE },
				{ BRCC, BRNE, BRPL, BRVC, BRGE, BRHC, BRTC, BRID }
			};

			uint32_t row = (raw & 0x0400) != 0; // 1111 0bxx xxxx xxxx ==> clear/set
			uint16_t col = (raw & 0x0007); // 1111 0xxx xxxx Xbbb ==> condition to test

			instr = SREG_BRANCH_LUT[row][col];

			break;
		}
		case 0xF800:
			if ((raw & 0xF) < 8)
				instr = BLD;
			break;
		case 0xFA00:
			if ((raw & 0xF) < 8)
				instr = BST;
			break;
		case 0xFC00:
			if ((raw & 0xF) < 8)
				instr = SBRC;
			break;
		case 0xFE00:
			if ((raw & 0xF) < 8)
				instr = SBRS;
			break;
	}

	return instr;
}

#ifdef DECODE_OP_INLINE
inline op_t ATTR_INLINE
#else
op_t
#endif
avr_decode(const hw_t *hw, uint32_t addr)
{
	op_t op;
	instr_t instr = UNDEF;

	uint16_t raw = flash_read_word(hw->flash, addr);

	switch (raw & 0xF000)
	{
		case 0x0000:
			instr = p_decode_row00(raw);
			break;
		case 0x1000:
			instr = p_decode_row01(raw);
			break;
		case 0x2000:
			instr = p_decode_row02(raw);
			break;
		case 0x3000:
			instr = CPI;
			break;
		case 0x4000:
			instr = SBCI;
			break;
		case 0x5000:
			instr = SUBI;
			break;
		case 0x6000:
			instr = ORI;
			break;
		case 0x7000:
			instr = ANDI;
			break;
		case 0x8000:
		case 0xA000:
			instr = p_decode_load_store(raw);
			break;
		case 0x9000:
			instr = p_decode_row09(raw);
			break;
		case 0xB000:
			instr = p_decode_in_out(raw);
			break;
		case 0xC000:
			instr = RJMP;
			break;
		case 0xD000:
			instr = RCALL;
			break;
		case 0xE000:
			instr = LDI;
			break;
		case 0xF000:
			instr = p_decode_branch(raw);
			break;
	}

	switch (instr)
	{
		case ADC: case ROL:
		case ADD: case LSL:
		case AND: case TST:
		case CPC:
		case CPSE:
		case EOR: case CLR:
		case LSR:
		case MOV:
		case MUL:
		case OR:
		case SUB:
		case SBC:
			GET_D5R5(op, raw);
			break;
		case ADIW:
		case SBIW:
			op.k = (raw & 0x00C0)>>2 | (raw & 0x000F);
			op.rd = 24 + ((raw & 0x0030)>>3);
			break;
		case ANDI: case CBR:
		case CPI:
		case LDI: case SER:
		case ORI: case SBR:
		case SUBI:
		case SBCI:
			GET_D4K8(op, raw);
			break;
		case BCLR:
		case CLC: case CLZ: case CLN: case CLV:
		case CLS: case CLH: case CLT: case CLI:
		case SEC: case SEZ: case SEN: case SEV:
		case SES: case SEH: case SET: case SEI:
			op.s = (raw & 0x0070)>>4;
			break;
		case FMUL: case FMULS: case FMULSU:
			op.rd = ((raw & 0x0070)>>4) + 16;
			op.rr = (raw & 0x0007) + 16;
			break;
		case MULS:
			op.rd = ((raw & 0x00F0)>>4) + 16;
			op.rr = (raw & 0x000F) + 16;
			break;
		case CBI:
		case SBI:
		case SBIS:
			op.b = (raw & 0x0007);
			op.a = (raw & 0x00F8)>>3;
			break;
		case RCALL:
		case RJMP:
			op.k = ((int32_t)(raw<<20))>>20;
			break;
		case ASR:
		case COM:
		case DEC:
		case INC:
		case LD:
		case LPM:
		case NEG:
		case POP:
		case ROR:
		case SWAP:
			GET_R5(op.rd, raw);
			break;
		case PUSH:
			GET_R5(op.rr, raw);
			break;
		case BLD:
		case BST:
			GET_R5(op.rd, raw);
			op.b = (raw & 7);
			break;
		case SBRC:
		case SBRS:
			GET_R5(op.rr, raw);
			op.b = (raw & 7);
			break;
		case BRBC:
		case BRCC: case BRNE: case BRPL: case BRVC:
		case BRGE: case BRHC: case BRTC: case BRID:
		case BRSH:
		case BRCS: case BREQ: case BRMI: case BRVS:
		case BRLT: case BRHS: case BRTS: case BRIE:
		case BRLO:
			op.k = ((int16_t)(raw<<6))>>9;
			op.s = (raw & 7);
			break;
		case LDD:
		case STD:
		{
			// 10q0 qqIr rrrr Rqqq
			// I: LDD=0, STD=1
			// R: Z=0, Y=1

			uint8_t reg = (raw & 0x01F0) >> 4;
			if (raw & 0x0200)
				op.rr = reg;
			else
				op.rd = reg;

			op.q = ((raw & 0x2000)>>8) |
				((raw & 0x0C00)>>7) |
				((raw & 0x0007));

			break;
		}
		case IN:
		case OUT:
		{
			uint8_t r = (raw & 0x01F0) >> 4;
			if (raw & 0x0800)
				op.rr = r;
			else
				op.rd = r;
			op.a = ((raw & 0x0600)>>5) | (raw & 0x000F);
			break;
		}
		case CALL:
		case JMP:
		{
			// TODO: Intentionally crash the emulator when the last opcode
			// in memory decodes to a 32-bit instruction.
			uint16_t raw_lo32 = flash_read_word(hw->flash, addr+1);
			op.k = ((raw & 0x01F0)>>3) | (raw & 0x0001) | (raw_lo32);

			break;
		}
		case MOVW:
			op.rd = ((raw & 0x00F0)>>4)*2;
			op.rr = (raw & 0x000F)*2;
			break;
		default:
			break;
	}

	op.instr = instr;
	op.raw = raw;

	return op;
}
