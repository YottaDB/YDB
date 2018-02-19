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
#include "gtm_iconv.h"
#include "io.h"
#include "iosp.h"
#include "iotimer.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"
#include "ebc_xlat.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "send_msg.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "change_reg.h"
#include "setterm.h"
#include "getzposition.h"
#include "min_max.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

#define TMP_BUF_LEN	1

GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;
GBLREF spdesc		stringpool;

error_def(ERR_IONOTOPEN);

int op_rdone(mval *v, mval *timeout)
{
	char		*start_ptr,temp_buf[TMP_BUF_LEN];
	int	x;
	int4		msec_timeout;
	size_t		insize, outsize, stat;
	unsigned char	*temp_buf_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_MSTIMEOUT(timeout, msec_timeout, READTIMESTR);
	active_device = io_curr_device.in;
	x = -1;
	assert(SIZEOF(mint) == SIZEOF(x));
	stat = (io_curr_device.in->disp_ptr->rdone)((mint *)&x, msec_timeout);
#	if defined(KEEP_zOS_EBCDIC)
	if (DEFAULT_CODE_SET != active_device->in_code_set)
	{
		insize = outsize = 1;
		start_ptr = temp_buf;
		temp_buf[0] = x;
		temp_buf_ptr = (unsigned char *)temp_buf;
		ICONVERT(active_device->input_conv_cd, &temp_buf_ptr, &insize, &temp_buf_ptr, &outsize); /* 	in-place conv */
		x = start_ptr[0];
	}
#	endif
	MV_FORCE_MVAL(v, x);
	active_device = 0;
	return ((NO_M_TIMEOUT != msec_timeout) ? stat : FALSE);
}
