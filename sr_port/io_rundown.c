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

#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "error.h"

GBLREF io_log_name	*io_root_log_name;

GBLREF io_pair		io_std_device;
GBLREF bool		prin_in_dev_failure;
GBLREF bool		prin_out_dev_failure;

void io_dev_close (io_log_name *d);

void io_rundown (int rundown_type)
{
	io_log_name	*l;		/* logical name pointer	*/

	if (io_root_log_name == 0)
		return;
	for (l = io_root_log_name;  l != 0; l = io_root_log_name)
	{
		io_root_log_name = l->next;
		if (l->iod != 0)
		{
			if ((NORMAL_RUNDOWN == rundown_type) ||
			   ((RUNDOWN_EXCEPT_STD == rundown_type) &&
				((l->iod->pair.in != io_std_device.in) && (l->iod->pair.out != io_std_device.out))))
				io_dev_close(l);
		}
	}
}


void io_dev_close (io_log_name *d)
{
	static readonly unsigned char	p[] = {iop_rundown, iop_eol};
	mval				pp;

	if (d->iod->pair.in == io_std_device.in  &&  d->iod->pair.out == io_std_device.out)
	{
		if (prin_in_dev_failure || prin_out_dev_failure)
			return;
	}

	ESTABLISH(lastchance3);
	pp.mvtype = MV_STR;
	pp.str.addr = (char *) p;
	pp.str.len = SIZEOF(p);
	if (d->iod->pair.in && d->iod->pair.in->state == dev_open)
		(d->iod->pair.in->disp_ptr->close)(d->iod->pair.in, &pp);
	if (d->iod->pair.out && d->iod->pair.out->state == dev_open)
		(d->iod->pair.out->disp_ptr->close)(d->iod->pair.out, &pp);
	REVERT;
}
