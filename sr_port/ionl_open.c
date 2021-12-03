/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 52a92dfd (GT.M V7.0-001)
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "errno.h"
#include "gtm_unistd.h"

#include "io_params.h"
#include "io.h"
#include "stringpool.h"
#include "gtmio.h"
<<<<<<< HEAD
#include "copy.h"
=======
#include "op.h"
#include "indir_enum.h"
>>>>>>> 52a92dfd (GT.M V7.0-001)

#define DEF_NL_WIDTH 255
#define DEF_NL_LENGTH 66

LITREF unsigned char io_params_size[];

short ionl_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, uint8 timeout)
{
	unsigned char	ch;
	io_desc		*d_out, *ioptr;
	int		p_offset, status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	p_offset = 0;
	/* If UNIX, then /dev/null was actually opened by io_open_try so we have to close it
	   since we don't use the device, we just simulate it by doing nothing on writes except
	   maintaining the appropriate pointers. We test for fd >= 0 since the less than zero
	   values mean no device was opened.
	*/
	UNIX_ONLY(
		if (0 <= fd)
		        CLOSEFILE_RESET(fd, status);	/* resets "fd" to FD_INVALID */
	);
	ioptr = dev_name->iod;
	ioptr->state = dev_open;
	d_out = ioptr->pair.out;
	ioptr->length = DEF_NL_LENGTH;
	ioptr->width = DEF_NL_WIDTH;
	ioptr->wrap = TRUE;
	ioptr->dollar.za = 0;
	ioptr->dollar.zeof = FALSE;
	ioptr->dollar.x = 0;
	ioptr->dollar.y = 0;
	while (*(pp->str.addr + p_offset) != iop_eol)
	{
<<<<<<< HEAD
		ch = *(pp->str.addr + p_offset++);
		if (ch == iop_wrap)
			d_out->wrap = TRUE;
		if (ch == iop_nowrap)
			d_out->wrap = FALSE;
		if (ch == iop_exception)
=======
		switch (ch = *(pp->str.addr + p_offset++))
>>>>>>> 52a92dfd (GT.M V7.0-001)
		{
		case iop_wrap:
			d_out->wrap = TRUE;
			break;
		case iop_nowrap:
			d_out->wrap = FALSE;
			break;
		case iop_exception:
			DEF_EXCEPTION(pp, p_offset, ioptr);
			break;
		}
		UPDATE_P_OFFSET(p_offset, ch, pp);	/* updates "p_offset" using "ch" and "pp" */
	}
	return TRUE;
}
