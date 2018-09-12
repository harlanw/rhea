#include "rhea_utils.h"

int
hex2bin(char hex)
{
	int result = -1;

	if (hex >= '0' && hex <= '9')
		result = hex - '0';
	else if (hex >= 'a' && hex <= 'f')
		result = hex - 'a' + 10;
	else if (hex >= 'A' && hex <= 'F')
		result = hex - 'A' + 10;

	return result;
}
