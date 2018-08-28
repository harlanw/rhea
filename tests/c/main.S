	.file	"main.c"
__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__tmp_reg__ = 0
__zero_reg__ = 1
	.text
.global	foo
	.type	foo, @function
foo:
	push r28
	push r29
	rcall .
	in r28,__SP_L__
	in r29,__SP_H__
/* prologue: function */
/* frame size = 2 */
/* stack size = 4 */
.L__stack_usage = 4
	std Y+2,r25
	std Y+1,r24
	ldd r24,Y+1
	ldd r25,Y+2
	ldi r18,lo8(52)
	ldi r19,lo8(18)
	movw r30,r24
	std Z+1,r19
	st Z,r18
	nop
/* epilogue start */
	pop __tmp_reg__
	pop __tmp_reg__
	pop r29
	pop r28
	ret
	.size	foo, .-foo
.global	main
	.type	main, @function
main:
	push r28
	push r29
	rcall .
	in r28,__SP_L__
	in r29,__SP_H__
/* prologue: function */
/* frame size = 2 */
/* stack size = 4 */
.L__stack_usage = 4
	ldi r24,lo8(12)
	ldi r25,0
	std Y+2,r25
	std Y+1,r24
	ldd r24,Y+1
	ldd r25,Y+2
	adiw r24,3
	std Y+2,r25
	std Y+1,r24
	movw r24,r28
	adiw r24,1
	call foo
	ldi r24,lo8(-1)
	ldi r25,lo8(-1)
	std Y+2,r25
	std Y+1,r24
/* #APP */
 ;  20 "main.c" 1
	sleep;
 ;  0 "" 2
/* #NOAPP */
	ldi r24,0
	ldi r25,0
/* epilogue start */
	pop __tmp_reg__
	pop __tmp_reg__
	pop r29
	pop r28
	ret
	.size	main, .-main
	.ident	"GCC: (Gentoo 6.4.0-r1 p1.3) 6.4.0"
