#ifndef AVR_DECODE_H
#define AVR_DECODE_H

#include "hw/devices.h"

#include <stdint.h>

#define INSTR_IS_32(op) \
	(((op) == CALL) || ((op) == JMP))

enum avr_instr
{
	UNDEF = 0,

	/* ARITHMETIC */
	ADD, ADC, ADIW,
	LSL,			/* LSL => ADD */
	ROL,			/* ROL => ADC */
	SUB, SUBI, SBC, SBCI, SBIW,
	DEC, INC,
	MUL, MULS, MULSU,
	FMUL, FMULS, FMULSU,

	/* LOGIC */
	AND, ANDI, CBR,		/* CBR => ANDI */
	TST,			/* TST => AND */
	EOR,
	CLR,			/* CLR => EOR */
	COM, NEG,
	OR, ORI,
	SBR,			/* SBR => ORI */
	SER, 			/* SER => LDI */

	/* BRANCH */
	CALL, ICALL, RCALL, EICALL,
	JMP, IJMP, RJMP, EIJMP,
	RET, RETI,

	CP, CPI, CPC, CPSE,

	SBRC, SBRS,
	SBIC, SBIS,

	BRBS, /* MAPS TO BELOW */
	BRCS, BREQ, BRMI, BRVS, BRLT, BRHS, BRTS, BRIE,
	BRLO,			/* BRLO => BRCS */

	BRBC, /* MAPS TO BELOW */
	BRCC, BRNE, BRPL, BRVC, BRGE, BRHC, BRTC, BRID,
	BRSH,			/* BRSH => BRCC */

	/* Bit, Bit-Test */
	ASR, LSR, ROR,
	SWAP,
	SBI, CBI,

	BSET, /* MAPS TO BELOW */
	SEC, SEZ, SEN, SEV, SES, SEH, SET, SEI,
	BCLR, /* MAPS TO BELOW */
	CLC, CLZ, CLN, CLV, CLS, CLH, CLT, CLI,

	BLD, BST,

	/* Data Transfer */
	IN, OUT,
	LD, LDD, LDI, LDS,
	ST, STD, STS,
	LPM, SPM, ELPM,
	MOV, MOVW,
	PUSH, POP,

	/* MCU Control */
	BREAK, NOP, SLEEP, WDR,

	/* MISC. */
	DES, XCH
};

struct avr_opcode
{
	enum avr_instr instr;
	uint32_t raw;

	uint8_t rd;
	uint8_t rr;

	union
	{
		uint32_t k;
		uint8_t a;
		uint8_t q;
	};

	union
	{
		uint8_t b;
		uint8_t s;
	};
};

typedef struct avr_opcode op_t;
typedef enum avr_instr instr_t;

op_t avr_decode(const hw_t *hw, uint32_t addr);
const char *avr_op_str(enum avr_instr instr);

#endif
