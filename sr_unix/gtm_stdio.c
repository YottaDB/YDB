/****************************************************************
 *								*
 * Copyright (c) 2010-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "have_crit.h"
#include "eintr_wrappers.h"

#include "gtm_stdio.h"
#include "gtm_signal.h"

#include <stdarg.h>
#include <errno.h>

/* Collection of eintr wrapper routines for gtm_stdio.h declared routines. Since all of the routines have variable
 * length parameter lists, it is not possible to do the wrappers using macros. Some routines may also have additional
 * signal blocking implemented to prevent the routines from being re-entered due to signals (known to cause issues on
 * some platforms.
 *
 * Note for the routines that utilize signal blocks, blocksig_initialized may not be set at this point for a variety of
 * cases such one as the following:
 * 	1) If the current image is "dbcertify"
 * 	2) At startup of GTCM_SERVER or GTCM_GNP_SERVER
 * 	3) At startup of GT.M (e.g. bad command line "mumps -bad")
 * Because of this, we dont have an assert(blocksig_initialized) that similar code in dollarh.c has.
 *
 * Routines implemented:
 *   gtm_printf()
 *   gtm_fprintf()
 *   gtm_sprintf()
 *   gtm_snprintf()
 *   gtm_scanf()
 *   gtm_fscanf()
 *   gtm_sscanf()
 */

GBLREF	boolean_t	blocksig_initialized;
GBLREF  sigset_t	block_sigsent;

/* Wrapper for printf() */
int gtm_printf(const char *format, ...)
{
	va_list		printargs;
	int		retval;

	va_start(printargs, format);
	VPRINTF(format, printargs, retval);
	retval = (-1 == retval) ? errno : 0;
	va_end(printargs);
	return retval;
}

/* Wrapper for fprintf() */
int gtm_fprintf(FILE *stream, const char *format, ...)
{
	va_list		printargs, pa_copy;
	size_t		retval, buflen, retlen;
	sigset_t	savemask;
	char		*buf, tmpbuf[256];
	intrpt_state_t	prev_intrpt_state;

	retval = 0;
	va_start(printargs, format);
	VAR_COPY(pa_copy, printargs);
	VSNPRINTF(NULL, 0, format, pa_copy, buflen);	/* C99: NULL string just returns size */
	va_end(pa_copy);
	if (SIZEOF(tmpbuf) > buflen)	/* ">" (and not ">=") to account for terminating NULL byte */
		buf = tmpbuf;		/* Use C local variable for most common case (where formatted output is < 256 bytes) */
	else
		buf = malloc(buflen + 1); /* Use malloc/free for unusual case where formatted output is >= 256 bytes */
	VSNPRINTF(buf, buflen + 1, format, printargs, buflen);
	GTM_FWRITE(buf, 1, buflen, stream, retlen, retval);
	if (retlen < buflen)
	{	/* Error from "GTM_FWRITE". "retval" contains "errno". */
		assert(errno == retval);
		retval = -1;	/* Now that "errno" has been set, return -1 just like "fprintf" would in case of error */
	}
	va_end(printargs);
	if (buf != tmpbuf)
		free(buf);
	return retval;
}

/* Wrapper for sprintf() - unneeded / unwanted as it is not safe */

/* Wrapper for snprintf() */
int	gtm_snprintf(char *str, size_t size, const char *format, ...)
{
	va_list		printargs;
	int		retval;

	va_start(printargs, format);
	VSNPRINTF(str, size, format, printargs, retval);
	va_end(printargs);
	assert(-1 != retval);
	return retval;
}

/* Note these functions do not exist on TRU64 (OSF1) */
#ifndef __osf__
/* Wrapper for scanf() */
int	gtm_scanf(const char *format, ...)
{
	va_list		scanargs;
	int		retval;

	va_start(scanargs, format);
	VSCANF(format, scanargs, retval);
	va_end(scanargs);
	return retval;
}

/* Wrapper for fscanf() */
int	gtm_fscanf(FILE *stream, const char *format, ...)
{
	va_list		scanargs;
	int		retval;

	va_start(scanargs, format);
	VFSCANF(stream, format, scanargs, retval);
	va_end(scanargs);
	return retval;
}

/* Wrapper for sscanf() */
int	gtm_sscanf(char *str, const char *format, ...)
{
	va_list		scanargs;
	int		retval;

	va_start(scanargs, format);
	VSSCANF(str, format, scanargs, retval);
	va_end(scanargs);
	return retval;
}
#endif
