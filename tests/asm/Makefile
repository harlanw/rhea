default:
	avr-as -g --gstabs -mmcu=atmega128 -o main.o main.S
	avr-gcc -nostdlib -mmcu=atmega128 -o main.elf main.o
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-objdump -S main.elf
