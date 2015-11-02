/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mlkdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlk_garbage_collect.h"
#include "mlk_shrclean.h"
#include "mlk_shrsub_garbage_collect.h"

void mlk_garbage_collect(mlk_ctldata_ptr_t ctl,
			 uint4 size,
			 mlk_pvtblk *p)
{
	ptroff_t *d;

	if (ctl->subtop - ctl->subfree < size)
		mlk_shrsub_garbage_collect(ctl);

	if ((ctl->blkcnt < p->subscript_cnt) || (ctl->subtop - ctl->subfree < size))
	{
		d = (ptroff_t *) &ctl->blkroot;
		mlk_shrclean(p->region, ctl, (mlk_shrblk_ptr_t)R2A(*d));
		mlk_shrsub_garbage_collect(ctl);
	}

	return;
}

