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
#include "io.h"
#include "iottdef.h"
#include "io_params.h"

static readonly unsigned char home_param_list[] =
{
	(unsigned char)iop_x,
	0, 0,
	(unsigned char)iop_y,
	0, 0,
	(unsigned char)iop_clearscreen,
	(unsigned char)iop_eol
};

GBLREF io_pair io_curr_device;

void iott_wtff(void)
{
	mval	home_params;

	home_params.mvtype = MV_STR;
	home_params.str.len = sizeof(home_param_list) - 1;
	home_params.str.addr = (char *)home_param_list;	/* mstrs should hold unsigned chars, but until then stop compile warning */
	io_curr_device.out->esc_state = START;
	iott_use(io_curr_device.out, &home_params);
}
