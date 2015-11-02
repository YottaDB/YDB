/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtm_fcntl.h"
#include "job.h"
#include "eintr_wrappers.h"

#ifdef KEEP_ZOS_EBCDIC
extern unsigned char a2e[];
#define SLSH a2e['/']
#else
#define SLSH '/'
#endif

/*
 * --------------------------------------------------------
 * Check if file spec is legal.
 * If exist flag is on, check if file exist
 *
 * Return:
 *	FALSE - filespec is illegal, or
 *		file does not exist (if exist flag is ON)
 *	TRUE  - good filespec (and file exist, if exist flag is ON)
 * --------------------------------------------------------
 */
int4	ojchkfs (char *addr, int4 len, bool exist)
{
	char	*fnp, es[MAX_FILSPC_LEN];
	int	fclose_res;
	FILE	*fp;

	/* First, check for a legal filespec */
	if (len > MAX_FILSPC_LEN)
		return(FALSE);

	strncpy(es, addr, len);
	*(es + len) = '\0';

	/* If directory path exist, skip it */
	if (fnp = strrchr(es, SLSH))
		fnp += 1;
	else
		fnp = es;
	if (!exist)
		return(TRUE);

	if ((fp = Fopen(es, "r")) == 0)
		return (FALSE);
	FCLOSE(fp, fclose_res);

	return(TRUE);
}
