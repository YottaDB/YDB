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
#include "cmidef.h"
#include "gtm_string.h"

void cmj_init_clb(struct NTD *tsk, struct CLB *lnk)
{
	if (lnk)
	{
		if (tsk)
			memset(lnk, 0 , SIZEOF(*lnk) + tsk->usr_size );
		else
			memset(lnk, 0 , SIZEOF(*lnk));
		lnk->ntd = tsk;
		lnk->sta = CM_CLB_DISCONNECT;
		lnk->mun = -1;
		if (tsk && tsk->usr_size)
			lnk->usr = (void *)((char *)lnk + SIZEOF(*lnk));
	}
}
