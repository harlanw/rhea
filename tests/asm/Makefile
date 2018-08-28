default:
	avr-gcc -Os -mmcu=atmega328p -c main.S
	avr-gcc -nostdlib -mmcu=atmega328p -o main.elf main.o
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-objdump -S main.elf
