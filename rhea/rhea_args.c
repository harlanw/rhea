#include "rhea_args.h"

#include "app.h"
#include "attributes.h"
#include "util/terminal.h"

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

void ATTR_NORETURN
usage_exit(bool fail)
{
	int status = (fail) ? EXIT_FAILURE : EXIT_SUCCESS;

	printf("Usage: %s [...] --mcu=<device> FILE\n", g_app.name);
	for (size_t i = 0; i < N_OPTIONS; i++)
	{
		option_t *opt = &OPTIONS[i];
		if (opt->has_rhv)
			printf(BOLD("\t%s,%s") "\t- %s\n", opt->abbr, opt->name, opt->desc);
		else
			printf(BOLD("\t%s,%s") "\t\t- %s\n", opt->abbr, opt->name, opt->desc);
	}

	exit(status);
}

int
parse_args(char **args, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		char *arg = args[i];

		if (arg[0] != '-')
		{
			const char *filetype = strchr(arg, '.');

			g_app.upload.path = arg;
			g_app.upload.type = FT_NONE;

			if (filetype)
			{
				if (strcasecmp(filetype, ".hex") == 0)
					g_app.upload.type = FT_IHEX;
				else if (strcasecmp(filetype, ".elf") == 0)
					g_app.upload.type = FT_ELF;
			}

			continue;
		}

		size_t j;
		for (j = 0; j < N_OPTIONS; j++)
		{
			option_t *opt = &OPTIONS[j];
			bool is_abbr = false;

			// Check if there is a match and set the match type. For options
			// with right-hand values, this match is only a partial match, i.e.,
			// --option=FOO is only checked up UNTIL the equal sign. This means
			// that --optionFOO is also a "match."
			if ((is_abbr = strncmp(arg, opt->name, opt->nlen)) == 0 ||
				strncmp(arg, opt->abbr, opt->alen) == 0)
			{
				if (opt->has_rhv == false)
				{
					* (bool *) opt->value = true;
				}
				else
				{
					// substring locations
					size_t eq = (is_abbr) ? opt->alen : opt->nlen;
					size_t rval = eq + 1;

					// --option RVAL
					if (rval > strlen(arg))
					{
						// Check option bounds
						if (i + 1 <= count - 1)
						{
							* (char **) opt->value = args[i+1];
							++i;
						}
						else
						{
							return -1;
						}
					}
					// --option=RVAL
					else if (rval < strlen(arg))
					{
						// Has to look like --option=RVAL or -o=RVAL otherwise
						// --optionFOO and -oFOO would match when it should not.
						if (arg[eq] != '=')
							continue;

						* (char **) opt->value = &arg[rval];
					}
					else
					{
						return -1;
					}
				}

				break;
			}

			if (j == N_OPTIONS)
			{
				return -1;
			}
		}
	}

	return 0;
}
