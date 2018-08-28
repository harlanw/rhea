;
; @file   - main.S
; @author - Harlan Waldrop <waldroha@oregonstate.edu> 
;

#include <avr/io.h>

.org 0x0

_MAIN_:
	ldi r16, 0xff
	push r16
	rcall FOO
	pop r17

	sleep

FOO:
	push r16

	ldi r16, 0xFE
	ldi YH, 0x02
	LDI YL, 0x00

	ldi ZL, 0
	ldi ZH, 0
	LDD r14, Z+16

	st Y, r16

	pop r16

	ret

/*
.section .data
.org 0x30104
LOOP:
	ldi 	r16 	, 0xCC

*/
