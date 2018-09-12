#include "rhea_load.h"

#include "rhea_ihex.h"

int
rhea_load_file(file_t file, chunk_t **arp)
{
	int status = 0;

	switch (file.type)
	{
		case FT_IHEX:
			status = ihex_load(file.path, arp);
			break;
		default:
			status = -1;
	}

	return status;
}

void
rhea_unload_file(file_t file, chunk_t **arp, int n)
{
	switch (file.type)
	{
		case FT_IHEX:
			ihex_unload(arp, n);
			break;
		default:
			break;
	}
}
