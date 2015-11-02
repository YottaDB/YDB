/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "trace_table.h"
#include "parse_trctbl_groups.h"

#define TRACEGROUP(group) #group,
#define TRACETYPE(group, type, int, addr1, addr2, addr3)
LITDEF char *gtm_trcgrp_names[LAST_TRACE_GROUP + 1] =
{
	"NOT USED",
#	include "trace_table_types.h"
	"NOT USED"
};
#undef TRACEGROUP
#undef TRACETYPE

error_def(ERR_INVTRCGRP);

/* Parse trace table group list:
 *
 * Trace table groups are specified as "grpname1, grpname2, etc" with or without white space. Parse these names,
 * look 'em up and set the appropriate bit mask in TREF(gtm_trctbl_groups). This is not the fanciest parse on the
 * planet but given that this is a one time act in a process and only if we are tracing, it doesn't really need
 * to be fancy max-performance. Note special case if string is "ALL".
 */
void parse_trctbl_groups(mstr *grps)
{
	unsigned char	*grpstrt, *cp, *cpe;
	int		grplen, grpindx;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cp = (unsigned char *)grps->addr;
	cpe = cp + grps->len;				/* Terminating null is not part of parse string */
	assert((0 == grps->len) || ('\0' == *cpe));	/* Validate above assumption */
	if (0 == strcmp(grps->addr, "ALL"))
	{	/* We want all the trace groups - set all the mask bits */
		TREF(gtm_trctbl_groups) = 0xFFFFFFFF;
		return;
	}
	while (cp < cpe)
	{
		while ((cp < cpe) && ((',' == *cp) || (' ' == *cp) || ('\t' == *cp)))
			cp++;		/* Ignore white space and spurious commas */
		/* Now have start of a group name token */
		grpstrt = cp;
		/* Look for token terminator */
		while ((cp < cpe) && (',' != *cp) && ('\t' != *cp) && (' ' != *cp))
			cp++;		/* Look for char not white space or comma */
		/* Have end of token */
		grplen = cp - grpstrt;
		if (0 == grplen)
			break;		/* can happen if value had trailing space(s) and/or comma(s) */
		if ((cp < cpe) && (',' == *cp))
			cp++;		/* forward space past comma */
		/* See if this is a valid group name */
		for (grpindx = 1; LAST_TRACE_GROUP > grpindx; grpindx++)
		{
			if ((strlen(gtm_trcgrp_names[grpindx]) == grplen)
			    && (0 == memcmp(grpstrt, gtm_trcgrp_names[grpindx], grplen)))
			{	/* Group found - set related bit in mask */
				TREF(gtm_trctbl_groups) = (TREF(gtm_trctbl_groups) | (1 << grpindx));
				break;
			}
		}
		if (LAST_TRACE_GROUP == grpindx)
			/* We didn't find the group - raise error */
			rts_error(VARLSTCNT(4) ERR_INVTRCGRP, 2, grplen, grpstrt);
	}
}
