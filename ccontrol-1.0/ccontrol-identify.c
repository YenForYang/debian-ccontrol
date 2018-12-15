/* Routine to figure out what is running, whether distcc will parallelise. */
#include "ccontrol.h"
#include "stdrusty.h"
#include "extensions.c"
#include <stdlib.h>

static bool is_source(const char *sfile)
{
	const char *dot;

	dot = strrchr(sfile, '.');
	if (!dot)
		return false;

	return find_extension(dot+1, strlen(dot+1)) != NULL;
}

/* Tries to guess whether distcc will be able to distribute it. */
bool can_distcc(char **argv)
{
	unsigned int i;
	char *a, *input_file = NULL, *output_file = NULL;
	bool seen_opt_s = false, seen_opt_c = false;

	/* Stolen from distcc's dcc_scan_args,
	   Copyright (C) 2002, 2003, 2004 by Martin Pool <mbp@samba.org> */
	for (i = 1; (a = argv[i]); i++) {
		if (a[0] != '-') {
			if (is_source(a)) {
				if (input_file)
					return false;
				input_file = a;
			} else if (strends(a, ".o")) {
			got_output:
				if (output_file)
					return false;
				output_file = a;
			}
		} else {
			if (streq(a, "-E")) {
				return false;
			} else if (streq(a, "-MD") || streq(a, "-MMD")
				 || streq(a, "-MG") || streq(a, "-MP")) {
				;
			} else if (streq(a, "-MF") || streq(a, "-MT")
				 || streq(a,"-MQ")) {
				i++;
			} else if (a[1] == 'M') {
				return false;
			} else if (strstr(a, "-Wa,")) {
				if (strstr(a, ",-a") || strstr(a, "--MD"))
					return false;
			} else if (strstarts(a, "-specs=")) {
				return false;
			} else if (streq(a, "-S")) {
				seen_opt_s = true;
			} else if (streq(a, "-fprofile-arcs")
				 || streq(a, "-ftest-coverage")
				 || streq(a, "-frepo")) {
				return false;
			} else if (strstarts(a, "-x")) {
				return false;
			} else if (streq(a, "-c")) {
				seen_opt_c = true;
			} else if (streq(a, "-o")) {
				a = argv[++i];
				goto got_output;
			} else if (strstarts(a, "-o")) {
				a += 2;         /* skip "-o" */
				goto got_output;
			}
		}
	}

	if (!seen_opt_c && !seen_opt_s)
		return false;

	if (!input_file)
		return false;
	return true;
}

/* What am I? */
enum type what_am_i(char *argv[])
{
	const char *basename = strrchr(argv[0], '/');
	if (!basename)
		basename = argv[0];

	if (strstr(basename, "cc"))
		return TYPE_CC;
	else if (strstr(basename, "++"))
		return TYPE_CPLUSPLUS;
	else if (strstr(basename, "ld"))
		return TYPE_LD;
	else if (strstr(basename, "make"))
		return TYPE_MAKE;
	else
		return TYPE_OTHER;
}
