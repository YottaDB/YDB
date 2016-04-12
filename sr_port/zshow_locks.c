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
#include "locklits.h"
#include "mlkdef.h"
#include "zshow.h"
#include "mvalconv.h"

GBLREF	uint4		process_id;
GBLREF	mlk_pvtblk	*mlk_pvt_root;
GBLREF	mlk_stats_t	mlk_stats;			/* Process-private M-lock statistics */

void zshow_locks(zshow_out *output)
{
	static readonly char zal[] = "ZAL ";
	static readonly char lck[] = "LOCK ";
	static readonly char lvl[] = " LEVEL=";
	static readonly char mlg[] = "MLG:";	/* # of M Lock commands Granted   */
	static readonly char mlt[] = ",MLT:";	/* # of M Lock commands Timed-out */
	mval		v;
	mlk_pvtblk	*temp;
	unsigned char	valstr[MAX_DIGITS_IN_INT8];
	uchar_ptr_t	ptr;

	/* Print LUS statistic */
	output->flush = FALSE;
	v.str.addr = &mlg[0];
	v.str.len = SIZEOF(mlg) - 1;
	zshow_output(output, &v.str);
	ptr = i2ascl((uchar_ptr_t)valstr, mlk_stats.n_user_locks_success);
	v.str.len = (mstr_len_t)(ptr - &valstr[0]);
	v.str.addr = (char *)&valstr[0];
	zshow_output(output, &v.str);

	/* Print LUF statistic */
	output->flush = FALSE;
	v.str.addr = &mlt[0];
	v.str.len = SIZEOF(mlt) - 1;
	zshow_output(output, &v.str);
	ptr = i2ascl((uchar_ptr_t)valstr, mlk_stats.n_user_locks_fail);
	v.str.len = (mstr_len_t)(ptr - &valstr[0]);
	v.str.addr = (char *)&valstr[0];
	zshow_output(output, &v.str);
	output->flush = TRUE;
	v.str.len = 0;
	zshow_output(output,&v.str);

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
			v.str.len = SIZEOF(zal) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			output->flush = TRUE;
			v.str.len = 0;
			zshow_output(output,&v.str);
			output->flush = FALSE;
			v.str.addr = &lck[0];
			v.str.len = SIZEOF(lck) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			v.str.addr = &lvl[0];
			v.str.len = SIZEOF(lvl) - 1;
			zshow_output(output,&v.str);
			MV_FORCE_MVAL(&v,(int)temp->level) ;
			mval_write(output,&v,TRUE);
		} else if (temp->level)
		{
			v.str.addr = &lck[0];
			v.str.len = SIZEOF(lck) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			v.str.addr = &lvl[0];
			v.str.len = SIZEOF(lvl) - 1;
			zshow_output(output,&v.str);
			MV_FORCE_MVAL(&v,(int)temp->level) ;
			mval_write(output,&v,TRUE);
		} else if (temp->zalloc)
		{
			v.str.addr = &zal[0];
			v.str.len = SIZEOF(zal) - 1;
			zshow_output(output,&v.str);
			zshow_format_lock(output,temp);
			output->flush = TRUE;
			v.str.len = 0;
			zshow_output(output,&v.str);
		}
 	}
}
