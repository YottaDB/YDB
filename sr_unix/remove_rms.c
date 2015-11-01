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

#include "gtm_stdio.h"

#include "io.h"
#include "iormdef.h"

GBLREF	io_log_name	*io_root_log_name;

void remove_rms (io_desc *ciod)
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
#ifndef __MVS__
			assert (lp->iod->pair.out == ciod);
#else
			if (lp->iod->pair.out != ciod && rm_ptr->fifo)
			{
				free((lp->iod->pair.out)->dev_sp);
				free(lp->iod->pair.out);
			}
#endif
			/*
			 * The only device that may be "split" is the
			 * principal device.  Since that device is
			 * permanently open, it will never get here.
			 */
			*lpp = (*lpp)->next;
			free (lp);
		}
		else
			lpp = &lp->next;
	}
	free (rm_ptr);
	free (ciod);
}
