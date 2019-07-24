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

#include "gtm_signal.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#include "gtm_caseconv.h"
#include "min_max.h"
#include "ydb_sigfwd_init.h"
#include "gtmmsg.h"
#include "io.h"
#include "gtmio.h"
#include "generic_signal_handler.h"
#include "ydb_logicals.h"

LITREF char *ydbenvname[];

/* Define tables needed to look up names to turn them into actual signal values to forward or not. If these tables become useful
 * elsewhere in YDB, they should perhaps move to mtables.c though beware of what additional that brings in to executables needing
 * this code.
 *
 * Note this routine is driven from gtm_env_init_sp() so is run before error handling is setup. Because of that, we follow the
 * "pre-error handling protocol" and report the error to the console (stderr) and then we generally ignore it.
 *
 * Note these tables are sorted alphabetically so we can binary search them.
 */
#define YDBSIGNAL(SIGNAME, SIGVALUE) {(unsigned char *)SIGNAME, (sizeof(SIGNAME) - 1), SIGVALUE},
LITDEF signame_value signames[] = {
#include "ydb_sigfwd_table.h"
};
#undef YDBSIGNAL

#define SIGPREFIX	"SIG"
#define SIGBUFSIZ	16
/* List of restricted signals with likelihood of encountering them in order of appearance */
LITDEF int restrictedSignums[] = {SIGCHLD, SIGCONT, SIGKILL, SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU};
#define DIM_RESTRICTED_SIGNUMS (ARRAYSIZE(restrictedSignums))
/* List of initially disabled signals but which can be enabled if desired */
LITDEF int initiallyDisabledSignums[] = {SIGALRM, SIGUSR1};
#define DIM_INITIALLY_DISABLED_SIGNUMS (ARRAYSIZE(initiallyDisabledSignums))

GBLDEF unsigned char	sigfwdMask[(NSIG + 7) / 8];	/* Bitmask with one bit for each signal (1 origin) */

/* Initialize the signal forward mask (forward to non-M caller) such that all signals except the set of restricted signals
 * are initialized to be forwarded.
 */
void ydb_sigfwd_init(void)
{
	int i;

	/* Init all signals to enabled */
	memset(&sigfwdMask[0], 0xff, SIZEOF(sigfwdMask));
	/* Disable signals that are restricted (cannot be enabled) */
	for (i = 0; DIM_RESTRICTED_SIGNUMS > i; i++)
		DISABLE_SIGFWD_BIT(restrictedSignums[i]);
	/* Disable for signals we allow to be enabled but are not by default (as that causes problems in Go without a handler defined) */
	for (i = 0; DIM_INITIALLY_DISABLED_SIGNUMS > i; i++)
		DISABLE_SIGFWD_BIT(initiallyDisabledSignums[i]);
#	ifdef DEBUG
	/* Verify all of the array names specified in table will fit in SIGBUFSIZ */
	for (i = 0; ARRAYSIZE(signames) > i; i++)
		assert(SIGBUFSIZ > signames[i].signameLen);
#	endif
	return;
}

/* Set of routines to take a string with a list of signal names, their abbreviations or their values and turn the corresponding
 * bit on in the sigfwdMask or turn it off. Note forward requests for certain signals (CHLD, CONT, KILL, STOP, TSTP, TTIN, and
 * TTOU are never forwarded because they either can't be caught (KILL, STOP) or because doing so could compromise YDB's handling
 * of these signals.
 */

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

/* Routine to take a string of signals and either enable or disable them in the signal mask. If signals are not being enabled,
 * then the list is treated as a disable list. Each specified signal can either be a full signal name (e.g. SIGSEGV) or can be
 * an abbreviation (SEGV) or can be a number. If a number, the number must be less or equal to NSIG. Note we just send errors
 * to stderr here because error handling is not yet setup.
 */
void set_sigfwd_mask(unsigned char *siglist, int siglistLen, boolean_t enableSigs)
{
	unsigned char	*cptr, *ctop, *sigtoken, sigtokenBuf[SIGBUFSIZ];
	boolean_t	isnumeric, allowSet;
	int		sigvalue, indx, sigtokenLen, envindx;

	if (0 == siglistLen)
		return;		/* No list, nothing to do */
	DBGSIGHND((stderr, "set_sigfwd_mask: %s the signal set: %.*s\n", enableSigs ? "Enabling" : "Disabling", siglistLen,
		   siglist));
	envindx = enableSigs ? YDBENVINDX_SIGNAL_FWD : YDBENVINDX_SIGNAL_NOFWD;	/* Determine what ISV we are dealing with */
	cptr = siglist;
	ctop = siglist + siglistLen;
	while (cptr < ctop)
	{
		/* Find start of signal token (eliminate white space) */
		if ((' ' == *cptr) || ('\t' == *cptr))
		{
			cptr++;
			continue;
		}
		/* Have start of signal token - find end of it */
		sigtoken = cptr;
		isnumeric = TRUE;
		while (cptr < ctop)
		{
			if ((' ' == *cptr) || ('\t' == *cptr))
				break;
			if (('0' > *cptr) || ('9' < *cptr))
				isnumeric = FALSE;
			cptr++;
		}
		/* Found termination of token - calculate length */
		sigtokenLen = cptr - sigtoken;
		if (isnumeric)
		{
			if ((SIZEOF(sigtokenBuf) - 1) < sigtokenLen)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVSIGNM, 4,
					       STRLEN(ydbenvname[envindx]), ydbenvname[envindx],
					       sigtokenLen, sigtoken);
				continue;	/* Ignoring bad parm but continue scanning */
			}
			memcpy(&sigtokenBuf[0], sigtoken, sigtokenLen);	/* Move to our buffer so can set null terminator */
			sigtokenBuf[sigtokenLen] = '\0';
			sigvalue = atoi((char *)sigtokenBuf);
			if ((NSIG < sigvalue) || (0 >= sigvalue))	/* Value must be 0 < sigvalue <= NSIG */
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVSIGNM, 4,
					       STRLEN(ydbenvname[envindx]), ydbenvname[envindx],
					       sigtokenLen, sigtoken);
				continue;	/* Ignoring bad parm but continue scanning */
			}
		} else
		{
			sigvalue = signal_lookup(sigtoken, sigtokenLen);
			if (0 >= sigvalue)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVSIGNM, 4,
					       STRLEN(ydbenvname[envindx]), ydbenvname[envindx],
					       sigtokenLen, sigtoken);
				continue;
			}
			assert(NSIG >= sigvalue);
		}
		/* Now have the signal value but before we (un)set the appropriate bit in the mask, see if the signal is
		 * restricted and if so, ignore the request.
		 */
		allowSet = TRUE;
		for (indx = 0; DIM_RESTRICTED_SIGNUMS > indx; indx++)
		{
			if (sigvalue == restrictedSignums[indx])
			{
				allowSet = FALSE;
				DBGSIGHND((stderr, "set_sigfwd_mask: Signal %d is restricted so no update for this sig\n",
					   sigvalue));
				break;
			}
		}
		/* Now either set or unset the bit associated with this signal */
		if (allowSet)
		{
			if (enableSigs)
			{
				DBGSIGHND((stderr, "set_sigfwd_mask: Signal %d is being enabled for forwarding\n", sigvalue));
				ENABLE_SIGFWD_BIT(sigvalue);
			} else
			{
				DBGSIGHND((stderr, "set_sigfwd_mask: Signal %d is being disabled for forwarding\n", sigvalue));
				DISABLE_SIGFWD_BIT(sigvalue);
			}
		} else
		{	/* If signal if signal is restricted and cannot be forwarded, tell system log about it then ignore it */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVSIGNM, 4,
				       STRLEN(ydbenvname[YDBENVINDX_SIGNAL_FWD]), ydbenvname[YDBENVINDX_SIGNAL_FWD],
				       sigtokenLen, sigtoken);
		}
	}
#	ifdef DEBUG_SIGNAL_HANDLING
	DBGFPF((stderr, "set_sigfwd_mask: Signal forward mask (bit set means ok to forward): "));
	for (indx = 0; SIZEOF(sigfwdMask) > indx; indx++)
	{
		DBGFPF((stderr, " 0x%02x", sigfwdMask[indx]));
	}
	DBGSIGHND((stderr, "\nsetsigfwd_mask: complete\n"));
#	endif
}
