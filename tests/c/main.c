#include <avr/io.h>

void
foo(int *x)
{
	*x = 0x1234;

	return;
}

int
main(void)
{
	int x = 12;
	x = x + 3;

	foo(&x);

	x = 0xFFFF;

	__asm__("sleep;");

	return 0;
}
