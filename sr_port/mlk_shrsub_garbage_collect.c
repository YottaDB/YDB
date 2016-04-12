/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stddef.h>
#include "mdef.h"
#include "gtm_string.h"
#include "mlkdef.h"
#include "copy.h"
#include "mlk_shrsub_garbage_collect.h"

void mlk_shrsub_garbage_collect(mlk_ctldata_ptr_t ctl)
{
        ptroff_t                bckp;
	int4 			delta, n;
	mlk_shrsub_ptr_t	p;
	mlk_shrblk_ptr_t	b;
	sm_uc_ptr_t		f;

	delta = 0;
	for (p = (mlk_shrsub_ptr_t)R2A(ctl->subbase), f = (sm_uc_ptr_t)R2A(ctl->subfree);
		p < (mlk_shrsub_ptr_t) f ; p = (mlk_shrsub_ptr_t)((sm_uc_ptr_t) p + n))
	{
		n = (int4)MLK_SHRSUB_SIZE(p);
		bckp = p->backpointer;
		if (0 == bckp)
			delta += n;
		else if (0 != delta)
		{
			b = (mlk_shrblk_ptr_t)((sm_uc_ptr_t)&p->backpointer + bckp);
			b->value -= delta;
			bckp += delta;
			p->backpointer = bckp;
			memmove((sm_uc_ptr_t) p - delta, p, n);
		}
	}
	ctl->subfree -= delta;
	return;
}
