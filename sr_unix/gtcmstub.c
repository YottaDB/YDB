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


/*** STUB FILE ***/
#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "parse_file.h"
#include "gvcmy_open.h"
#include "gvcmy_rundown.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmx.h"
#include "gvcmz.h"
#include "mvalconv.h"

void gvcmy_rundown(void)
{
	return;
}

void gvcmy_open(gd_region *reg, parse_blk *nb)
{
	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);

	rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT,
		2, LEN_AND_LIT("This utility does not support remote database operations"));
}

mint gvcmx_data(void)
{
	assert(FALSE);
	return -1;
}

bool gvcmx_get(mval *v)
{
	assert (FALSE);
	return(-1);
}

void gvcmx_kill(bool do_subtree)
{
	assert (FALSE);
}

bool gvcmx_order(void)
{
	assert (FALSE);
	return(-1);
}

void gvcmx_put(mval *v)
{
	assert(FALSE);
}

bool gvcmx_query(mval *val)
{
	assert (FALSE);
	return(-1);
}

void gvcmz_clrlkreq(void)
{
	assert (FALSE);
}

void gvcmx_unlock(unsigned char rmv_locks, bool specific, char incr)
{
	assert (FALSE);
}

bool gvcmx_zprevious(void)
{
        assert (FALSE);
        return(-1);
}

void	gvcmx_increment(mval *increment, mval *result)
{
	assert(FALSE);
	return;
}

