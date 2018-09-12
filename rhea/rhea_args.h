#ifndef RHEA_ARGS_H
#define RHEA_ARGS_H

#include <stdbool.h>
#include <stddef.h>

#define OPT_PAIR(flag) flag, (sizeof(flag)-1)

typedef struct option
{
	const char *name;
	size_t nlen;

	const char *abbr;
	size_t alen;

	const char *desc;

	bool has_rhv;

	void *value;
} option_t;

extern option_t OPTIONS[];
extern size_t N_OPTIONS;

void usage_exit(bool fail);
int parse_args(char **args, size_t count);

#endif
