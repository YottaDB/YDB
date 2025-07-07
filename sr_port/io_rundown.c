/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "invocation_mode.h"
#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "iott_setterm.h"
#include "error.h"

GBLREF boolean_t	prin_in_dev_failure, prin_out_dev_failure;
GBLREF io_log_name	*io_root_log_name;
GBLREF io_pair		io_std_device;

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
			else if ((tt == l->iod->pair.in->type) && (MUMPS_CALLIN == invocation_mode)
				&& (RUNDOWN_EXCEPT_STD == rundown_type)
				&& (l->iod->pair.in == io_std_device.in))
				iott_resetterm(l->iod->pair.in);		/* restore termios attributes if stdin and tt */
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
		if (prin_out_dev_failure || (prin_in_dev_failure && (io_std_device.in == io_std_device.out)))
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
