#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "utils.h"

extern void
die(const char *fmt, ...)
{
	va_list args;

	fputs("xchalkboard: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	exit(1);
}
