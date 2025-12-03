/****************************************************************
 *                                                              *
 * Copyright (c) 2025 Fidelity National Information             *
 * Services, Inc. and/or its subsidiaries. All rights reserved. *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/
#ifndef BACKTRACE_INCLUDED
#define BACKTRACE_INCLUDED

/*
 * The BACKTRACE macro allows calls to the function invoking it
 * to be traced with the Linux "backtrace" facility.  In general
 * the output produced here will need post processing as the
 * GT.M compile environment doesn't enable full support for backtrace.
 *
 * Invoking the macro on AIX is harmless, but does nothing as AIX
 * does not provide backtrace support.
 *
 * Three issues to consider:
 *   1) The backtrace facility uses malloc() & free() so we must bypass
 *      gtm_malloc()/gtm_free(), which may be an issue in some cases.
 *
 *   2) Outputting the data uses fprintf(), and so may not be appropriate
 *      if a function may be called in a signal handler.
 *
 *   3) Outputting the backtrace information involves a file open/file close
 *      and file i/o on every call, so execution times may be affected,
 *      and the output file may be quite large.
 */
#ifdef __linux__
#include "gtm_stdio.h"
#include <dlfcn.h>
#include <execinfo.h>
#define ignore_typedef typedef

#define BACKTRACE(DEPTH, FUNC_NAME, OFILE)					\
MBSTART {									\
										\
        void *callstack[DEPTH];							\
        int frames;								\
        char **strs = NULL;							\
        void *my_ptr = FUNC_NAME;						\
	FILE *fp;								\
	int i;									\
	ignore_typedef void (*free_func_t)(void *);				\
	free_func_t free_func;							\
										\
	free_func = (free_func_t) dlsym(RTLD_DEFAULT, "free");			\
										\
										\
        frames = backtrace(callstack, DEPTH);					\
        strs = backtrace_symbols(callstack, frames);				\
										\
	/* We are not going to error out just because we can't output */	\
        fp = fopen(OFILE, "a");							\
	if (fp)									\
	{									\
		fprintf(fp, "%p ", my_ptr);					\
		for (i = 0; i < frames; ++i) {					\
			fprintf(fp, "%s ", strs[i]);				\
		}								\
		fprintf(fp, "\n");						\
		(void) fclose(fp);						\
	}									\
	{									\
		/* backtrace_symbols uses system malloc(),			\
		 * so we must use system free() */				\
		if (strs)							\
        		(*free_func)(strs);					\
	}									\
} MBEND	
#else
/* AIX */
#define BACKTRACE(DEPTH, FUNC_NAME, OFILE)
#endif

#endif /* BACKTRACE_INCLUDED */
