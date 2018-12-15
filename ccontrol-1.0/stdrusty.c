#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <limits.h>

#include "stdrusty.h"

const char *int_to_string(int val)
{
	unsigned int ord, i;
	static char buf[2][CHAR_SIZE(val)];
	static int which;
	char *p = buf[which];

	if (val < 0) {
		*(p++) = '-';
		val = -val;
	}

	for (i = 0, ord = 1000000; ord; ord /= 10) {
		if (val >= ord || i != 0) {
			p[i++] = '0' + val / ord;
			val %= ord;
		}
	}
	if (i == 0)
		p[i++] = '0';
	p[i] = '\0';
	which = !which;
	return buf[!which];
}

void fatal(const char *msg, int err, ...)
{
	const char *str;
	va_list arglist;

	write(STDERR_FILENO, "ccontrol error: ", strlen("ccontrol error: "));
	write(STDERR_FILENO, msg, strlen(msg));

	va_start(arglist, err);
	while ((str = va_arg(arglist, const char *)) != NULL)
		write(STDERR_FILENO, str, strlen(str));
	va_end(arglist);
	if (err) {
		str = int_to_string(err);
		write(STDERR_FILENO, ": ", strlen(": "));
		write(STDERR_FILENO, str, strlen(str));
	}
	write(STDERR_FILENO, "\n", 1);
	exit(1);
}

#ifndef SIZE_MAX
#define SIZE_MAX UINT_MAX
#endif

void *_realloc_array(void *ptr, size_t size, size_t num)
{
        if (num >= SIZE_MAX/size)
                return NULL;
        return realloc_nofail(ptr, size * num);
}

void *realloc_nofail(void *ptr, size_t size)
{
        ptr = realloc(ptr, size);
	if (ptr)
		return ptr;
	fatal("realloc failed", errno, NULL);
}

/* This version adds one byte (for nul term) */
void *suck_file(int fd, unsigned long *size)
{
	unsigned int max = 16384;
	int ret, savederr = 0;
	void *buffer;

	if (fd < 0)
		return NULL;

	buffer = malloc(max+1);
	*size = 0;
	while ((ret = read(fd, buffer + *size, max - *size)) > 0) {
		*size += ret;
		if (*size == max)
			buffer = realloc(buffer, max *= 2 + 1);
		else
			/* Should never get short reads on file. */
			break;
	}
	if (ret < 0) {
		savederr = errno;
		free(buffer);
		buffer = NULL;
	} else
		((char *)buffer)[*size] = '\0';
	errno = savederr;
	return buffer;
}

void release_file(void *data, unsigned long size)
{
	free(data);
}
