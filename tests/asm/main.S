;
; @file   - main.S
; @author - Harlan Waldrop <waldroha@oregonstate.edu> 
;

#include <avr/io.h>

.org 0x0

main:
	ldi r17, 0x13
	bst r17, 4
	sleep
