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
#include "locklits.h"
#include "mlkdef.h"
#include "zshow.h"
#include "mvalconv.h"

GBLREF uint4 process_id;
GBLREF mlk_pvtblk *mlk_pvt_root;

void zshow_locks(zshow_out *output)
{
	static readonly char zal[] = "ZAL ";
	static readonly char lck[] = "LOCK ";
	static readonly char lvl[] = " LEVEL=";
	mval v;
	mlk_pvtblk *temp;

	for (temp = mlk_pvt_root; temp; temp = temp->next)
	{
		if (!temp->granted)
			continue;

		if (temp->nodptr && (temp->nodptr->owner != process_id
			|| temp->sequence != temp->nodptr->sequence))
			continue;

		output->flush = FALSE;
		if (temp->level && temp->zalloc)
		{
			v.str.addr = &zal[0];
			v.str.len = sizeof(zal) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			output->flush = TRUE;
			v.str.len = 0;
			zshow_output(output,&v.str);
			output->flush = FALSE;
			v.str.addr = &lck[0];
			v.str.len = sizeof(lck) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			v.str.addr = &lvl[0];
			v.str.len = sizeof(lvl) - 1;
			zshow_output(output,&v.str);
			MV_FORCE_MVAL(&v,temp->level) ;
			mval_write(output,&v,TRUE);
		}
		else if (temp->level)
		{
			v.str.addr = &lck[0];
			v.str.len = sizeof(lck) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			v.str.addr = &lvl[0];
			v.str.len = sizeof(lvl) - 1;
			zshow_output(output,&v.str);
			MV_FORCE_MVAL(&v,temp->level) ;
			mval_write(output,&v,TRUE);
		}else if (temp->zalloc)
		{
			v.str.addr = &zal[0];
			v.str.len = sizeof(zal) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			output->flush = TRUE;
			v.str.len = 0;
			zshow_output(output,&v.str);
		}
 	}
}
