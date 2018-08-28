#include "runtime/decode.c"

// TODO: Does not currently check that instruction exists on hardware

#include <stdio.h>
#include <stdlib.h>

uint8_t SRAW[] = { 0x00, 0x00, 0x00, 0x00 };

int
main(void)
{
	hw_t hw = { .flash = SRAW, .pc = 0, .flashend = 3};

	for (uint32_t i = 0; i <= 0xFFFF; i++)
	{
		hw.flash[0] = (i & 0xFF);
		hw.flash[1] = i >> 8; 

		op_t op = avr_decode(&hw, 0);
		instr_t exp, was = op.instr;

		exp = UNDEF;

		switch (i & 0xF000)
		{
			case 0x0000:
			{
				if (i == 0)
					exp = NOP;
				else if (IN_RANGE(i, 0x0100, 0x01FF))
					exp = MOVW;
				else if (IN_RANGE(i, 0x0200, 0x02FF))
					exp = MULS;
				else if (IN_RANGE(i, 0x0300, 0x03FF))
				{
					if ((i & 0x00F0) < 0x80)
					{
						if ((i & 0x000F) < 0x8)
							exp = MULSU;
						else
							exp = FMUL;
					}
					else
					{
						if ((i & 0x000F) < 0x8)
							exp = FMULS;
						else
							exp = FMULSU;
					}
				}
				else if (IN_RANGE(i, 0x0400, 0x07FF))
					exp = CPC;
				else if (IN_RANGE(i, 0x0800, 0x0BFF))
					exp = SBC;
				else if (IN_RANGE(i, 0x0C00, 0x0FFF))
					exp = ADD;
				break;
			}
			case 0x1000:
			{
				if (IN_RANGE(i, 0x1000, 0x13FF))
					exp = CPSE;
				else if (IN_RANGE(i, 0x1400, 0x17FF))
					exp = CP;
				else if (IN_RANGE(i, 0x1800, 0x1BFF))
					exp = SUB;
				else if (IN_RANGE(i, 0x1C00, 0x1FFF))
					exp = ADC;

				break;
			}
			case 0x2000:
			{
				if (IN_RANGE(i, 0x2000, 0x23FF))
					exp = AND;
				else if (IN_RANGE(i, 0x2400, 0x27FF))
					exp = EOR;
				else if (IN_RANGE(i, 0x2800, 0x2BFF))
					exp = OR;
				else if (IN_RANGE(i, 0x2C00, 0x2FFF))
					exp = MOV;

				break;
			}
			case 0x3000: exp = CPI; break;
			case 0x4000: exp = SBCI; break;
			case 0x5000: exp = SUBI; break;
			case 0x6000: exp = ORI; break;
			case 0x7000: exp = ANDI; break;
			case 0x8000:
			{
				if (IN_RANGE(i, 0x8000, 0x81FF))
					exp = LDD;
				else if (IN_RANGE(i, 0x8200, 0x83FF))
					exp = STD;
				else if (IN_RANGE(i, 0x8400, 0x85FF))
					exp = LDD;
				else if (IN_RANGE(i, 0x8600, 0x87FF))
					exp = STD;
				else if (IN_RANGE(i, 0x8800, 0x89FF))
					exp = LDD;
				else if (IN_RANGE(i, 0x8A00, 0x8BFF))
					exp = STD;
				else if (IN_RANGE(i, 0x8C00, 0x8DFF))
					exp = LDD;
				else if (IN_RANGE(i, 0x8E00, 0x8FFF))
					exp = STD;

				break;
			}
			case 0x9000:
			{
				if (IN_RANGE(i, 0x9000, 0x91FF))
				{
					switch (i & 0xF)
					{
						case 0:
							exp = LDS;
							break;
						case 1: case 2:
						case 9: case 10:
						case 12: case 13: case 14:
							exp = LD;
							break;
						case 4: case 5:
							exp = LPM;
							break;
						case 6: case 7:
							exp = ELPM;
							break;
						case 15:
							exp = POP;
							break;
					}
				}
				else if (IN_RANGE(i, 0x9200, 0x93FF))
				{
					switch (i & 0xF)
					{
						case 0:
							exp = STS;
							break;
						case 1: case 2:
						case 9: case 10:
						case 12: case 13: case 14:
							exp = ST;
							break;
						case 15:
							exp = PUSH;
							break;
					}
				}
				else if (IN_RANGE(i, 0x9400, 0x95FF))
				{
					switch (i & 0xF)
					{
						case 0: exp = COM; break;
						case 1: exp = NEG; break;
						case 2: exp = SWAP; break;
						case 3: exp = INC; break;
						case 4: /* RES */ break;
						case 5: exp = ASR; break;
						case 6: exp = LSR; break;
						case 7: exp = ROR; break;
						case 8:
							if ((i & 0x0F00) == 0x400)
							{
								switch ((i & 0xF0)>>4)
								{
									case 0: exp = SEC; break;
									case 1: exp = SEZ; break;
									case 2: exp = SEN; break;
									case 3: exp = SEV; break;
									case 4: exp = SES; break;
									case 5: exp = SEH; break;
									case 6: exp = SET; break;
									case 7: exp = SEI; break;
									case 8: exp = CLC; break;
									case 9: exp = CLZ; break;
									case 10: exp = CLN; break;
									case 11: exp = CLV; break;
									case 12: exp = CLS; break;
									case 13: exp = CLH; break;
									case 14: exp = CLT; break;
									case 15: exp = CLI; break;
								}
							}
							else if ((i & 0x0F00) == 0x500)
							{
								switch ((i & 0xF0)>>4)
								{
									case 0: exp = RET; break;
									case 1: exp = RETI; break;
									case 8: exp = SLEEP; break;
									case 9: exp = BREAK; break;
									case 10: exp = WDR; break;
									case 12: exp = LPM; break;
									case 13: exp = ELPM; break;
									case 14: exp = SPM; break;
									case 15: exp = SPM; break;
								}
							}
							break;
						case 9:
							if (i == 0x9409)
								exp = IJMP;
							else if (i == 0x9419)
								exp = EIJMP;
							else if (i == 0x9509)
								exp = ICALL;
							else if (i == 0x9519)
								exp = EICALL;
							break;
						case 10: exp = DEC; break;
						case 11: exp = (NIBBLE(i, 2) == 4) ? DES : UNDEF; break;
						case 12: case 13:
							exp = JMP;
							break;
						case 14: case 15:
							exp = CALL;
							break;

					}
				}
				else if (IN_RANGE(i, 0x9600, 0x96FF))
					exp = ADIW;
				else if (IN_RANGE(i, 0x9700, 0x97FF))
					exp = SBIW;
				else if (IN_RANGE(i, 0x9800, 0x98FF))
					exp = CBI;
				else if (IN_RANGE(i, 0x9900, 0x99FF))
					exp = SBIC;
				else if (IN_RANGE(i, 0x9A00, 0x9AFF))
					exp = SBI;
				else if (IN_RANGE(i, 0x9B00, 0x9BFF))
					exp = SBIS;
				else if (IN_RANGE(i, 0x9C00, 0x9FFF))
					exp = MUL;
				break;
			}
			case 0xA000:
				if (IN_RANGE(i, 0xA000, 0xA1FF))
					exp = LDD;
				else if (IN_RANGE(i, 0xA200, 0xA3FF))
					exp = STD;
				else if (IN_RANGE(i, 0xA400, 0xA5FF))
					exp = LDD;
				else if (IN_RANGE(i, 0xA600, 0xA7FF))
					exp = STD;
				else if (IN_RANGE(i, 0xA800, 0xA9FF))
					exp = LDD;
				else if (IN_RANGE(i, 0xAA00, 0xABFF))
					exp = STD;
				else if (IN_RANGE(i, 0xAC00, 0xADFF))
					exp = LDD;
				else if (IN_RANGE(i, 0xAE00, 0xAFFF))
					exp = STD;
				break;
			case 0xB000:
				if (IN_RANGE(i, 0xB000, 0xB7FF))
					exp = IN;
				else if (IN_RANGE(i, 0xB800, 0xBFFF))
					exp = OUT;
				break;
			case 0xC000:
				exp = RJMP;
				break;
			case 0xD000:
				exp = RCALL;
				break;
			case 0xE000:
				exp = LDI;
				break;
			case 0xF000:
				if (IN_RANGE(i, 0xF000, 0xF3FF))
				{
					switch (i & 0x000F)
					{
						case 0: exp = BRCS; break;
						case 1: exp = BREQ; break;
						case 2: exp = BRMI; break;
						case 3: exp = BRVS; break;
						case 4: exp = BRLT; break;
						case 5: exp = BRHS; break;
						case 6: exp = BRTS; break;
						case 7: exp = BRIE; break;
						case 8: exp = BRCS; break;
						case 9: exp = BREQ; break;
						case 10: exp = BRMI; break;
						case 11: exp = BRVS; break;
						case 12: exp = BRLT; break;
						case 13: exp = BRHS; break;
						case 14: exp = BRTS; break;
						case 15: exp = BRIE; break;
					}
				}
				else if (IN_RANGE(i, 0xF400, 0xF7FF))
				{
					switch (i & 0x000F)
					{
						case 0: exp = BRCC; break;
						case 1: exp = BRNE; break;
						case 2: exp = BRPL; break;
						case 3: exp = BRVC; break;
						case 4: exp = BRGE; break;
						case 5: exp = BRHC; break;
						case 6: exp = BRTC; break;
						case 7: exp = BRID; break;
						case 8: exp = BRCC; break;
						case 9: exp = BRNE; break;
						case 10: exp = BRPL; break;
						case 11: exp = BRVC; break;
						case 12: exp = BRGE; break;
						case 13: exp = BRHC; break;
						case 14: exp = BRTC; break;
						case 15: exp = BRID; break;
					}
				}
				else if (IN_RANGE(i, 0xF800, 0xF9FF) && (i & 0xF) < 8)
				{
					exp = BLD;
				}
				else if (IN_RANGE(i, 0xFA00, 0xFBFF) && (i & 0xF) < 8)
				{
					exp = BST;
				}
				else if (IN_RANGE(i, 0xFC00, 0xFDFF) && (i & 0xF) < 8)
				{
					exp = SBRC;
				}
				else if (IN_RANGE(i, 0xFE00, 0xFFFF) && (i & 0xF) < 8)
				{
					exp = SBRS;
				}
				break;
		}

		if (was != exp)
		{
			const char *str0 = avr_instr_str(was);
			const char *str1 = avr_instr_str(exp);

			printf("0x%.4X: was %s, expected %s :(\n", i, str0, str1);

			return EXIT_FAILURE;
		}
	}

	printf("\e[1mAll tests in '%s' passed!\e[0m\n", __FILE__);

	return EXIT_SUCCESS;
}
