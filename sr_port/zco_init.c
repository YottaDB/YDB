/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "iosp.h"
#include "io.h"
#include "gtm_logicals.h"
#include "trans_log_name.h"
#include "zco_init.h"

GBLREF	mstr	dollar_zcompile;

void zco_init (void)
{
	uint4	status;
	mstr		val, tn;
	char		buf1[MAX_TRANS_NAME_LEN]; /* buffer to hold translated name */

	if (dollar_zcompile.addr)
		free (dollar_zcompile.addr);

	val.addr = ZCOMPILE;
	val.len = sizeof(ZCOMPILE) - 1;
	status = trans_log_name(&val, &tn, buf1);
	if (status != SS_NORMAL && status != SS_NOLOGNAM)
		rts_error(VARLSTCNT(1) status);

	if (status == SS_NOLOGNAM)
		dollar_zcompile.len = 0;
	else
	{
		dollar_zcompile.len = tn.len;
		dollar_zcompile.addr = (char *) malloc (tn.len);
		memcpy (dollar_zcompile.addr, buf1, tn.len);
	}
}
