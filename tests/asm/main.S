;
; @file   - main.S
; @author - Harlan Waldrop <waldroha@oregonstate.edu> 
;

#include <avr/io.h>

.org 0x0

main:
	tst r0
	ldi r16, 0xFE

	call foo

	break


foo:
	ldi r16, 0xEF
	ret
