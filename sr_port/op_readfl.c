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
#include "iosp.h"
#include "iotimer.h"
#include "stringpool.h"
#include "op.h"
#include "ebc_xlat.h"

GBLREF io_pair		io_curr_device;
GBLREF spdesc		stringpool;
GBLREF io_desc		*active_device;

int op_readfl(mval *v, int4 length, int4 timeout)
{
	int4	stat;		/* status */
	size_t	cnt, insize, outsize;
	char	*start_ptr, *save_ptr;
	unsigned char	*temp_ch;
	error_def(ERR_TEXT);
	error_def(ERR_RDFLTOOSHORT);

	if (timeout < 0)
		timeout = 0;
	if (length <= 0)
		rts_error(VARLSTCNT(1) ERR_RDFLTOOSHORT);
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	if (stringpool.free + length + ESC_LEN > stringpool.top)
		stp_gcol(length + ESC_LEN);
	v->mvtype = MV_STR;
	v->str.addr = (char *)stringpool.free;
	v->str.len = 0;
	active_device = io_curr_device.in;
	stat = (io_curr_device.in->disp_ptr->readfl)(v, length, timeout);
	stringpool.free += v->str.len;
	assert((int4)v->str.len <= length);
	assert(stringpool.free <= stringpool.top);

	if (DEFAULT_CODE_SET != active_device->in_code_set)
	{
		cnt = insize = outsize = v->str.len;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.free <= stringpool.top);
		if (cnt > stringpool.top - stringpool.free)
			stp_gcol(cnt);
		temp_ch = stringpool.free;
		save_ptr = v->str.addr;
		start_ptr = (char *)temp_ch;
		stringpool.free += cnt;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.free <= stringpool.top);

		ICONVERT(active_device->input_conv_cd, (unsigned char **)&(v->str.addr), &insize, &temp_ch, &outsize);

		v->str.addr = start_ptr;
	}

	active_device = 0;
	if (NO_M_TIMEOUT != timeout)
		return (stat);
	return FALSE;
}
