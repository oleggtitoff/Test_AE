/*
 * GetOpt.h
 *
 *  Created on: Aug 13, 2019
 *      Author: Intern_2
 */

#ifndef GETOPT
#define GETOPT

#include <string.h>

#define ALWAYS_INLINE static inline __attribute__((always_inline))


ALWAYS_INLINE int getOpt(const int argc, char *const argv[], const char *optstring, int *optind, char **optarg)
{
	if ((*optind >= argc) || (argv[*optind][0] != '-') || (argv[*optind][0] == 0))
	{
		return -1;
	}

	int opt = argv[*optind][1];
	const int8_t *p = strchr(optstring, opt);

	if (p == NULL)
	{
		printf("Unknown option %s\n", argv[*optind]);
		return '?';
	}

	if (p[1] == ':')
	{
		(*optind)++;

		if (*optind >= argc || (argv[*optind][0] == '-' && (argv[*optind][1] < 48 || argv[*optind][1] > 57)))
		{
			printf("Option %s needs a value\n", argv[*optind - 1]);
			return ':';
		}

		*optarg = argv[*optind];
	}

	(*optind)++;
	return opt;
}

#endif /* GETOPT */
