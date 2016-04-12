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
#include "iotimer.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"
#include "ebc_xlat.h"

#define TMP_BUF_LEN	1

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;
GBLREF spdesc		stringpool;

int op_rdone(mval *v, int4 timeout)
{
	int4	stat;
	int	x;

	char	temp_buf[TMP_BUF_LEN];
	unsigned char	*temp_buf_ptr;
	char	*start_ptr;
	size_t	insize, outsize;
	error_def(ERR_IONOTOPEN);

	active_device = io_curr_device.in;
	if (timeout < 0)
		timeout = 0;
	x = -1;
	assert(SIZEOF(mint) == SIZEOF(x));
	stat = (io_curr_device.in->disp_ptr->rdone)((mint *)&x, timeout);

#if defined(KEEP_zOS_EBCDIC) || defined(VMS)
	if (DEFAULT_CODE_SET != active_device->in_code_set)
	{
		insize = outsize = 1;
		start_ptr = temp_buf;
		temp_buf[0] = x;
		temp_buf_ptr = (unsigned char *)temp_buf;
		ICONVERT(active_device->input_conv_cd, &temp_buf_ptr, &insize, &temp_buf_ptr, &outsize); /* 	in-place conv */
		x = start_ptr[0];
	}
#endif

	MV_FORCE_MVAL(v, x);
	active_device = 0;
	if (timeout != NO_M_TIMEOUT)
		return (stat);
	return FALSE;
}
