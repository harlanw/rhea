#ifndef RHEA_APP_H
#define RHEA_APP_H

#include "rhea_file.h"

typedef struct app
{
	const char *name;

	bool debug;
	bool help;
	bool verbose;

	const char *mcu;

	file_t log;
	file_t upload;
} app_t;

extern app_t g_app;

#endif
