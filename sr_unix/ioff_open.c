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

#include <unistd.h>

#include "gtm_stat.h"
#include "gtm_stdio.h"

#include "io.h"
#include "iormdef.h"
#include "io_params.h"

extern int errno;
GBLREF io_pair io_curr_device;

short ioff_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
{
	io_desc		*iod;
 	d_rm_struct	*d_rm;
	short		iorm_open();

	iod = dev_name->iod;
	assert((params) *(pp->str.addr) < (unsigned char) n_iops);
	assert(iod != 0);
	assert(iod->state >= 0 && iod->state < n_io_dev_states);
	assert(iod->type == ff);
	if (!(d_rm = (d_rm_struct *) iod->dev_sp))
	{	iod->dev_sp = (void*)malloc(sizeof(d_rm_struct));
		d_rm = (d_rm_struct *) iod->dev_sp;
		iod->state = dev_closed;
                d_rm->stream = FALSE;
                iod->width = DEF_RM_WIDTH;
                iod->length = DEF_RM_LENGTH;
                d_rm->fixed = FALSE;
                d_rm->noread = FALSE;
	}
	d_rm->fifo = TRUE;
	iod->type = rm;
	return iorm_open(dev_name, pp, fd, mspace, timeout);
}
