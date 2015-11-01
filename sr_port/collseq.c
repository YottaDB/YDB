/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

GBLDEF	collseq		*local_collseq = (collseq*)0;
GBLDEF	char		*lcl_coll_xform_buff; /* This buffer is meant to be
					       * used for local collation
					       * transformations. Local
					       * collation transformations are
					       * assumed to be not nested, i.e.,
					       * a transformation routine
					       * should not call another, or
					       * itself. This kind of nesting
					       * will cause the buffer to be
					       * overwritten */

/* The max size of the local collation buffer that will be extended from 32K each time the buffer overflows */
GBLDEF	int		max_lcl_coll_xform_bufsiz;

GBLDEF	collseq		*collseq_list = (collseq*)0;
GBLDEF  bool		transform;
GBLDEF	boolean_t	local_collseq_stdnull = FALSE;

int find_local_colltype(void)
{
	int	lct, status;
	char	transbuf[MAX_TRANS_NAME_LEN];
	mstr	lognam, transnam;

	lognam.len = sizeof(LCT_PREFIX) - 1;
	lognam.addr = LCT_PREFIX;
	status = trans_log_name(&lognam,&transnam,transbuf);
	if (status != SS_NORMAL) return 0;
	lct = asc2i((uchar_ptr_t)transnam.addr, transnam.len);
	return lct >= MIN_COLLTYPE && lct <= MAX_COLLTYPE ? lct : 0;
}

collseq *ready_collseq(int act)
{
	unsigned char	filespec[sizeof(CT_PREFIX) + 4];	/* '4' to hold the chars in the max allowable
								 * collation sequence (255) plus the terminating null */
	unsigned char	*fsp;
	collseq		temp_csp, *csp;
	mstr		fspec;

	/* Validate the alternative collating type (act) */
	if (!(act >= 1 && act <= MAX_COLLTYPE))
		return (collseq*)NULL;

	/* Search for record of the collating type already being mapped in. */
	for (csp = collseq_list; csp != NULL && act != csp->act; csp = csp->flink)
		;

	if (NULL == csp)
	{
		/* If not found, create a structure and attempt to map in the collating support package.*/
		temp_csp.act = act;
		temp_csp.flink = collseq_list;
		memcpy(filespec, CT_PREFIX, sizeof(CT_PREFIX));
		fsp = i2asc(&filespec[sizeof(CT_PREFIX) - 1], act);
		*fsp = 0;
		fspec.len =  fsp - filespec;
		fspec.addr = (char *)filespec;
		if (!map_collseq(&fspec, &temp_csp))
			return NULL;
		csp = (collseq *) malloc(sizeof(collseq));
		*csp = temp_csp;
		collseq_list = csp;
	}
	return csp;
}
