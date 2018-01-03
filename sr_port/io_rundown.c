/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

void io_dev_close(io_log_name *d);

void io_rundown (int rundown_type)
{
	io_log_name	*l;		/* logical name pointer	*/

	if (NULL == io_root_log_name)
		return;
	for (l = io_root_log_name;  NULL != l; free(l), l = io_root_log_name)
	{
		io_root_log_name = l->next;
		if ((NULL != l->iod) && (n_io_dev_types == l->iod->type))
		{	/* Device setup has started but did not complete (e.g. SIG-15 during device set up took us to exit handler)
			 * Not much can be done in terms of rundown of this device so skip it.
			 */
			continue;
		}
		if (NULL != l->iod)
		{
			if ((NORMAL_RUNDOWN == rundown_type)
				|| ((RUNDOWN_EXCEPT_STD == rundown_type)
					&& ((l->iod->pair.in != io_std_device.in) && (l->iod->pair.out != io_std_device.out))))
				io_dev_close(l);
		}
	}
}

void io_dev_close (io_log_name *d)
{
	static readonly unsigned char	p[] = {iop_rundown, iop_eol};
	io_desc				*iod;
	mval				pp;
#	ifdef DEBUG
	int				close_called;
#	endif

	iod = d->iod;
	if (iod->pair.in == io_std_device.in  &&  iod->pair.out == io_std_device.out)
	{
		if (prin_in_dev_failure || prin_out_dev_failure)
			return;
	}
	pp.mvtype = MV_STR;
	pp.str.addr = (char *) p;
	pp.str.len = SIZEOF(p);
	DEBUG_ONLY(close_called = 0;)
	if (iod->pair.in && (iod->pair.in->state == dev_open))
	{
		DEBUG_ONLY(close_called = __LINE__;)
		(iod->pair.in->disp_ptr->close)(iod->pair.in, &pp);
	}
	if (iod->pair.out && (iod->pair.out->state == dev_open))
	{
		DEBUG_ONLY(close_called = __LINE__;)
		(iod->pair.out->disp_ptr->close)(iod->pair.out, &pp);
	}
	if (iod->newly_created)
	{
		assert(0 == close_called);
		if (gtmsocket == iod->type)
			iosocket_destroy(iod);
		else if (rm == iod->type)
			remove_rms(iod);
		else
			assert(FALSE);	/* Not sure what to cleanup */
	}
}
