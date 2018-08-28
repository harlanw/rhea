;
; @file   - main.S
; @author - Harlan Waldrop <waldroha@oregonstate.edu> 
;

#include <avr/io.h>

.org 0x0

main:
	ldi r16, 0xFE
	ldi ZL, pm_lo8(foo)
	ldi ZH, pm_hi8(foo)

	icall

	sleep

foo:
	ldi r17, 0xED

	ret
