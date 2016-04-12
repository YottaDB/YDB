/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "io.h"
#include "iosp.h"
#include "collseq.h"
#include "error.h"
#include "trans_log_name.h"
#include "gtm_logicals.h"

int find_local_colltype(void)
{
	int	lct, status;
	char	transbuf[MAX_TRANS_NAME_LEN];
	mstr	lognam, transnam;

	lognam.len = SIZEOF(LCT_PREFIX) - 1;
	lognam.addr = LCT_PREFIX;
	status = TRANS_LOG_NAME(&lognam, &transnam, transbuf, SIZEOF(transbuf), do_sendmsg_on_log2long);
	if (SS_NORMAL != status)
		return 0;
	lct = asc2i((uchar_ptr_t)transnam.addr, transnam.len);
	return lct >= MIN_COLLTYPE && lct <= MAX_COLLTYPE ? lct : 0;
}

collseq *ready_collseq(int act)
{
	unsigned char	filespec[SIZEOF(CT_PREFIX) + 4];	/* '4' to hold the chars in the max allowable
								 * collation sequence (255) plus the terminating null */
	unsigned char	*fsp;
	collseq		temp_csp, *csp;
	mstr		fspec;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Validate the alternative collating type (act) */
	if (!(act >= 1 && act <= MAX_COLLTYPE))
		return (collseq*)NULL;
	/* Search for record of the collating type already being mapped in. */
	for (csp = TREF(collseq_list); csp != NULL && act != csp->act; csp = csp->flink)
		;
	if (NULL == csp)
	{
		/* If not found, create a structure and attempt to map in the collating support package.*/
		temp_csp.act = act;
		temp_csp.flink = TREF(collseq_list);
		memcpy(filespec, CT_PREFIX, SIZEOF(CT_PREFIX));
		fsp = i2asc(&filespec[SIZEOF(CT_PREFIX) - 1], act);
		*fsp = 0;
		fspec.len =  INTCAST(fsp - filespec);
		fspec.addr = (char *)filespec;
		if (!map_collseq(&fspec, &temp_csp))
			return NULL;
		csp = (collseq *) malloc(SIZEOF(collseq));
		*csp = temp_csp;
		TREF(collseq_list) = csp;
	}
	return csp;
}
