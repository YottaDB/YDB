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

#include "mdef.h"
#include <fab.h>
#include <rab.h>
#include <nam.h>
#include "io.h"
#include "iormdef.h"

GBLREF	io_log_name	*io_root_log_name;

void remove_rms ( io_desc *ciod)
{
	io_log_name	**lpp, *lp;	/* logical name pointers */
	d_rm_struct     *rm_ptr;

	assert (ciod->type == rm);
	assert (ciod->state == dev_closed || ciod->state == dev_never_opened);
	rm_ptr = (d_rm_struct *) ciod->dev_sp;
	for (lpp = &io_root_log_name, lp = *lpp; lp; lp = *lpp)
	{
		if (lp->iod->pair.in == ciod)
		{
			assert (lp->iod == ciod);
			*lpp = (*lpp)->next;
			free (lp);
		}
		else
			lpp = &lp->next;
	}
	assert (rm_ptr->inbuf);
	assert (rm_ptr->outbuf_start);
	if (rm_ptr->f.fab$l_nam)
		free(rm_ptr->f.fab$l_nam);
	free (rm_ptr->outbuf_start);
	free (rm_ptr->inbuf);
	free (rm_ptr);
	free (ciod);
}
