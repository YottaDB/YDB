/****************************************************************
 *								*
 * Copyright (c) 2012-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_socket.h"

#include "io.h"
#include "iosocketdef.h"
#include "gtmio.h"

GBLREF	io_log_name	*io_root_log_name;

void iosocket_destroy (io_desc *ciod)
{
	io_log_name	**lpp, *lp;	/* logical name pointers */
	d_socket_struct	*dsocketptr;
	assertpro(ciod->type == gtmsocket);
	assertpro(ciod->state == dev_closed);
	dsocketptr = (d_socket_struct *) ciod->dev_sp;
	assertpro(NULL != dsocketptr);
	for (lpp = &io_root_log_name, lp = *lpp; lp; lp = *lpp)
	{
		if ((NULL != lp->iod) && (n_io_dev_types == lp->iod->type))
		{
			assert(FALSE);
			continue;       /* skip it on pro */
		}
		if (lp->iod->pair.in == ciod)
		{
			/* The only device that may be "split" is the principal device. Since it is permanently open,
			 * it will never get here.
			 */
			assert(lp->iod == ciod);
			assert(lp->iod->pair.out == ciod);
			*lpp = (*lpp)->next;
			free(lp);
		}
		else
			lpp = &lp->next;
	}
	if (ciod->dollar.devicebuffer)
	{
		free(ciod->dollar.devicebuffer);
		ciod->dollar.devicebuffer = NULL;
	}
	free(dsocketptr);
	free(ciod);
}
