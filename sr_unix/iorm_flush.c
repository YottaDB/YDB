/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

void iorm_flush(io_desc *iod)
{
	d_rm_struct	*rm_ptr;
	unsigned int	*dollarx_ptr;

	rm_ptr = (d_rm_struct *)iod->dev_sp;

#ifdef __MVS__
	/* on zos if it is a fifo device then point to the pair.out for $X and $Y */
	if (rm_ptr->fifo)
		dollarx_ptr = &(iod->pair.out->dollar.x);
	else
#endif
		dollarx_ptr = &(iod->dollar.x);

	if (*dollarx_ptr && rm_ptr->lastop == RM_WRITE && !iod->dollar.za)
		iorm_wteol(1,iod);
	return;
}

