#include "logging.h"

#include <stdarg.h>
#include <stdio.h>

#include "globals.h"

int
rhea_log(FILE *stream, const char *fmt, ...)
{
	int status = -1;

	if (g_debug || g_verbose || stream == stderr)
	{
		va_list args;
		va_start(args, fmt);
		status = vfprintf(stream, fmt, args);
		va_end(args);
	}

	return status;
}
