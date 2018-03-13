/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"

#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "error.h"
#include "iottdef.h"
#include "setterm.h"

GBLREF	io_log_name	*io_root_log_name;
GBLREF	int		process_exiting;
GBLREF	uint4		process_id;
GBLREF	io_pair		io_std_device;
GBLREF	bool		prin_in_dev_failure;
GBLREF	bool		prin_out_dev_failure;

void io_dev_close(io_log_name *d);

void io_rundown (int rundown_type)
{
	io_log_name	*l;		/* logical name pointer	*/
	io_desc		*ioptr, *iod[2];
	int		i;

	if (NULL == io_root_log_name)
		return;
	for (l = io_root_log_name;  NULL != l; free(l), l = io_root_log_name)
	{
		io_root_log_name = l->next;
		ioptr = l->iod;
		if (NULL == ioptr)
			continue;
		if (n_io_dev_types == ioptr->type)
		{	/* Device setup has started but did not complete (e.g. SIG-15 during device set up took us to exit handler)
			 * Not much can be done in terms of rundown of this device so skip it.
			 */
			continue;
		}
		if ((NORMAL_RUNDOWN == rundown_type)
				|| ((RUNDOWN_EXCEPT_STD == rundown_type)
					&& ((ioptr->pair.in != io_std_device.in) && (ioptr->pair.out != io_std_device.out))))
			io_dev_close(l);
		else if (process_exiting)
		{	/* ioptr could have input or output pointing to a terminal. And YottaDB might have set some terminal
			 * settings as part of writing to terminal. If so, reset those before exiting YottaDB code.
			 */
			iod[0] = ioptr->pair.in;
			iod[1] = ioptr->pair.out;
			for (i = 0; i < 2; i++)
			{
				ioptr = iod[i];
				/* If standard device is a terminal and is open, check if "setterm" had been done.
				 * If so do "resetterm" to restore terminal to what it was (undo whatever stty settings
				 * YottaDB changed in terminal).
				 */
				if (IS_SETTERM_DONE(ioptr))
					resetterm(ioptr);
			}
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
