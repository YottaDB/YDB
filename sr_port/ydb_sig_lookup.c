/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_caseconv.h"
#include "ydb_sig_lookup.h"
#include "generic_signal_handler.h"

/* Define tables needed to look up names to turn them into actual signal values to forward or not. If these tables become useful
 * elsewhere in YDB, they should perhaps move to mtables.c though beware of what additional that brings in to executables needing
 * this code.
 *
 * Note these tables are sorted alphabetically so we can binary search them.
 */
#define YDBSIGNAL(SIGNAME, SIGVALUE) {(unsigned char *)SIGNAME, (sizeof(SIGNAME) - 1), SIGVALUE},
LITDEF signame_value signames[] = {
#include "ydb_sig_lookup_table.h"
};
#undef YDBSIGNAL

#define SIGPREFIX	"SIG"
#define SIGBUFSIZ	16
/* Routine to find a given name in the table(s) and return the signal value - note table is searched twice - once for the signal
 * name spelled out in full and if not found, is searched a second time for its abbreviation (sans the SIG prefix). If the signal
 * name is not found, returns -1 instead of a valid signal value.
 */
int signal_lookup(unsigned char *signame, int signameLen)
{
	int		botIndx, topIndx, midIndx, compRslt, csignameLen;
	unsigned char	signameBuf[SIGBUFSIZ], *lsigname, *csigname;
	if (signameLen > (sizeof(signameBuf) - 1))
		return -1;	/* If doesn't fit in our buffer, is not a valid signal name */
	lower_to_upper(&signameBuf[0], signame, signameLen);
	lsigname = &signameBuf[0];
	if ((STRLEN(SIGPREFIX) <= signameLen) && (0 == MEMCMP_LIT(lsigname, SIGPREFIX)))
	{	/* Buffer begins with "SIG" so search full table */
		for (botIndx = 0, topIndx = ARRAYSIZE(signames) - 1; botIndx <= topIndx; )
		{
			midIndx = (botIndx + topIndx) / 2;
			if (signames[midIndx].signameLen != signameLen)
			{	/* The lengths are different - need to compare and then recheck lengths to see which is greater */
				compRslt = memcmp(signames[midIndx].signame, lsigname,
						  MIN(signameLen, signames[midIndx].signameLen));
				if (0 == compRslt)
				{	/* If the strings are the same for the minimum length, then the longer string is greater */
					if (signames[midIndx].signameLen > signameLen)
						compRslt = 1; 	/* First string was longer */
					else
						compRslt = -1;	/* First string was shorter */
				} /* Else, compRslt is set appropriately already */
			} else
			{	/* Lengths are equal so compare the strings */
				compRslt = memcmp(signames[midIndx].signame, lsigname, signameLen);
			}
			if (0 > compRslt)
				botIndx = midIndx + 1;
			else if (0 == compRslt)
				return signames[midIndx].sigvalue;
			else
				topIndx = midIndx - 1;
		}
	} else
	{	/* Evidently not a full signal name so check for abbreviations instead */
		for (botIndx = 0, topIndx = ARRAYSIZE(signames) - 1; botIndx <= topIndx; )
		{
			midIndx = (botIndx + topIndx) / 2;
			csigname = signames[midIndx].signame + STRLEN(SIGPREFIX);	/* Point to abbreviation part of name */
			csignameLen = signames[midIndx].signameLen - STRLEN(SIGPREFIX);	/* .. and adjust the length */
			if (csignameLen != signameLen)
			{	/* The lengths are different - need to compare and then recheck lengths to see which is greater */
				compRslt = memcmp(csigname, lsigname, MIN(signameLen, csignameLen));
				if (0 == compRslt)
				{	/* If the strings are the same for the minimum length, then the longer string is greater */
					if (csignameLen > signameLen)
						compRslt = 1; 	/* First string was longer */
					else
						compRslt = -1;	/* First string was shorter */
				} /* Else, compRslt is set appropriately already */
			} else
			{	/* Lengths are equal so compare the strings */
				compRslt = memcmp(csigname, lsigname, signameLen);
			}
			if (0 > compRslt)
				botIndx = midIndx + 1;
			else if (0 == compRslt)
				return signames[midIndx].sigvalue;
			else
				topIndx = midIndx - 1;
		}
	}
	return -1;
}

