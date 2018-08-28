default:
	avr-gcc -mmcu=atmega328p -c main.c
	avr-gcc -mmcu=atmega328p -o main.elf main.o
	avr-objcopy -O ihex main.elf main.hex
