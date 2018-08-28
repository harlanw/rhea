/**
 * Copyright 2018 Harlan J. Waldrop <harlan@ieee.org>
 *
 * @file main.c
 * @author Harlan J. Waldrop <harlan@ieee.org>
 *
 * @brief rhea - a fully-featured command line emulator for the megax8 family.
 */

#include "globals.h"
#include "hw/devices.h"
#include "runtime/emu.h"
#include "runtime/jit.h"
#include "util/ihex.h"
#include "util/logging.h"

#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPT_PAIR(name) name, (sizeof(name)-1)

bool g_debug = false;
bool g_verbose = false;
char *g_logpath = NULL;

struct flags
{
	bool debug;
	bool help;
	bool verbose;

	char *logpath;
	char *filepath;
	char *filetype;

	char *mcu;
} flags = { 0 };

struct option
{
	/* Short Flag (-f) */
	char *flag;
	size_t flen;

	/* Long Flag (--flag) */
	char *name;
	size_t nlen;

	/* Flag Type: 0 = bool, 1 = string */
	size_t type;

	void *value;

	char *desc;
};

static struct option OPTIONS[] =
{
	{ OPT_PAIR("-h"),	OPT_PAIR("--help"),	0,	&flags.help,	"prints this menu and exits" },
	{ OPT_PAIR("-d"),	OPT_PAIR("--debug"),	0,	&flags.debug,	"turns on certain debug features" },
	{ OPT_PAIR("-v"),	OPT_PAIR("--verbose"),	0,	&flags.verbose,	"direct verbose messages to stdout" },
	{ OPT_PAIR("-l="),	OPT_PAIR("--log="),	1,	&flags.logpath,	"output path for log file" },
	{ OPT_PAIR("-m="),	OPT_PAIR("--mcu="),	1,	&flags.mcu,	"sets the target microcontroller, e.g., 328p" }
};

static size_t N_OPTIONS = sizeof(OPTIONS)/sizeof(OPTIONS[0]);

static void
display_usage(const char *bin, int status)
{
	printf("Usage: %s [...] FILE\n", bin);
	for (size_t i = 0; i < N_OPTIONS; i++)
	{
		struct option *opt = &OPTIONS[i];

		printf("\t%s,%s\t- %s\n", opt->flag, opt->name, opt->desc);
	}

	exit(status);
}

static bool
parse_args(char **args, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		char *arg = args[i];

		if (arg[0] != '-')
		{
			flags.filepath = arg;
			flags.filetype = strchr(arg, '.');

			if (flags.filetype)
				flags.filetype += 1;

			continue;
		}

		size_t j;
		for (j = 0; j < N_OPTIONS; j++)
		{
			struct option *opt = &OPTIONS[j];

			size_t is_flag = 0;

			// Check both formats, i.e., -o, --option
			if ((is_flag = !strncmp(arg, opt->flag, opt->flen)) || !strncmp(arg, opt->name, opt->nlen))
			{
				size_t len = (is_flag) ? opt->flen : opt->nlen;

				if (opt->type)
					* (char **) opt->value = &arg[len];
				else
					* (size_t *) opt->value = 1;

				break;
			}
		}

		if (j == N_OPTIONS)
		{
			ERROR("Unrecognized option '%s'", arg);
			return false;
		}
	}

	return true;
}

int
main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;

	char *appname = basename(argv[0]);

	uint8_t *flash;

	if (argc == 1)
	{
		display_usage(appname, EXIT_FAILURE);
	}

	++argv; --argc;
	if (!parse_args(argv, argc))
	{
		display_usage(appname, EXIT_FAILURE);
	}

	g_debug = flags.debug;
	g_verbose = flags.verbose;
	g_logpath = flags.logpath;

	if (flags.help)
		display_usage(appname, EXIT_SUCCESS);
	else if (flags.filepath == NULL)
		display_usage(appname, EXIT_FAILURE);
	else if (!flags.mcu)
	{
		ERROR("Must run with option --mcu=<DEVICE>");
		display_usage(appname, EXIT_FAILURE);
	}
	else if (strcasecmp(flags.filetype, "hex"))
	{
		ERROR("Currently only the IHEX file format is supported");
		exit(EXIT_FAILURE);
	}

	chunk_t *chunks = NULL;
	uint32_t n_chunks = avr_load(flags.filepath, &chunks);

	emu_t *emu = emu_init(flags.mcu, chunks, n_chunks);

	emu_run(emu);

	avr_unload(&chunks, n_chunks);
	emu_destroy(&emu);

	return status;
}
