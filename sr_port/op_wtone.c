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
#include "stringpool.h"
#include "io.h"
#include "iosp.h"
#include "op.h"
#include "ebc_xlat.h"

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;
GBLREF spdesc		stringpool;

static size_t		insize;
static size_t		outsize;

void op_wtone(unsigned char c)
{
	char	temp_ch[1];
	char	*start_ptr;
	unsigned char	*temp_ch_ptr;

	active_device = io_curr_device.out;

	if (DEFAULT_CODE_SET != active_device->out_code_set)
	{
		insize = outsize = 1;
		temp_ch[0] = c;
		temp_ch_ptr = (unsigned char *)temp_ch;
		start_ptr = temp_ch;
		ICONVERT(active_device->output_conv_cd, &temp_ch_ptr, &insize, &temp_ch_ptr, &outsize);
		c = start_ptr[0];
	}
	(io_curr_device.out->disp_ptr->wtone)(c);
	active_device = 0;
}
