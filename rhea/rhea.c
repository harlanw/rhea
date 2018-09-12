#include "rhea_args.h"

#include "app.h"
#include "rhea_load.h"
#include "runtime/emu.h"

#include <libgen.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define DIE(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)

app_t g_app = { 0 };

option_t OPTIONS[] =
{
	/* FLAGS */
	{ OPT_PAIR("--help"),    OPT_PAIR("-h"), "prints this menu and exits",         0, &g_app.help },
	{ OPT_PAIR("--debug"),   OPT_PAIR("-d"), "enables certain debugging features", 0, &g_app.debug },
	{ OPT_PAIR("--verbose"), OPT_PAIR("-v"), "enables verbose messages",           0, &g_app.verbose },

	/* STRINGS */
	{ "--mcu=<device>", 5,   OPT_PAIR("-m"), "sets emulation target",              1, &g_app.mcu },
};

size_t N_OPTIONS = sizeof(OPTIONS) / sizeof(OPTIONS[0]);

static emu_t *emu;

static void
die_gracefully()
{
	emu_destroy(&emu);
	exit(EXIT_SUCCESS);
}

static void
handle_signal(int no)
{
	if (no == SIGINT)
		die_gracefully();
}

int
main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;

	if (argc == 1)
	{
		usage_exit(1);
	}

	g_app.name = basename(argv[0]);

	++argv; --argc;
	if (parse_args(argv, argc) == -1)
	{
		usage_exit(1);
	}

	if (g_app.help)
		usage_exit(0);
	else if (g_app.upload.path == NULL)
		DIE("Must include option --mcu=<device>");
	else if (g_app.upload.type == FT_NONE)
		DIE("Must include upload file");
	else if (g_app.upload.type != FT_IHEX)
		DIE("Only Intel HEX files can be uploaded at this time");

	chunk_t *chunks;
	int n = rhea_load_file(g_app.upload, &chunks);
	if (n == -1)
	{
		return EXIT_FAILURE;
	}

	signal(SIGINT, handle_signal);

	//emu_t *emu = emu_init(g_app.mcu, chunks, n_chunks);
	emu = emu_init(g_app.mcu, chunks, n);
	rhea_unload_file(g_app.upload, &chunks, n);

	emu_run(emu);
	emu_destroy(&emu);

	return status;
}
