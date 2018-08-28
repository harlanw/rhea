;
; @file   - main.S
; @author - Harlan Waldrop <waldroha@oregonstate.edu> 
;

#include <avr/io.h>

.org 0x0

main:
	ldi ZL, lo8(pm(STR))
	ldi ZH, hi8(pm(STR))

	sleep

.section .text
STR:
.word 0xDEAD
