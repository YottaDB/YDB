/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_iconv.h"
#include "io.h"
#include "iosp.h"
#include "stringpool.h"
#include "op.h"
#include "ebc_xlat.h"

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;

void op_write(mval *v)
{
	GBLREF spdesc stringpool;
	size_t insize, outsize;
	int cnt;
	unsigned char *temp_ch;
	char *start_ptr, *save_ptr;

	MV_FORCE_STR(v);
	active_device = io_curr_device.out;

#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
	if (DEFAULT_CODE_SET != active_device->out_code_set)
	{
		cnt = insize = outsize = v->str.len;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.free <= stringpool.top);
		ENSURE_STP_FREE_SPACE(cnt);
		temp_ch = stringpool.free;

		save_ptr = v->str.addr;
		start_ptr = (char *)temp_ch;
		stringpool.free += cnt;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.free <= stringpool.top);

		ICONVERT(active_device->output_conv_cd, (unsigned char **)&v->str.addr, &insize, &temp_ch, &outsize);

		v->str.addr = start_ptr;
	}
#endif

	(io_curr_device.out->disp_ptr->write)(&v->str);

#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
	if (DEFAULT_CODE_SET != active_device->out_code_set)
		v->str.addr = save_ptr;
#endif

	active_device = 0;
	return;
}
