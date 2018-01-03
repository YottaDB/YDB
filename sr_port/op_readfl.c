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
#include "iotimer.h"
#include "stringpool.h"
#include "op.h"
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
#include "mvalconv.h"
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

GBLREF boolean_t	gtm_utf8_mode;
GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;
GBLREF spdesc		stringpool;

error_def(ERR_RDFLTOOSHORT);
error_def(ERR_RDFLTOOLONG);
error_def(ERR_TEXT);

int op_readfl(mval *v, int4 length, mval *timeout)
{
	char		*start_ptr, *save_ptr;
	int		b_length;
	int4		stat;		/* status */
	int4		msec_timeout;
	size_t		cnt, insize, outsize;
	unsigned char	*temp_ch;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_MSTIMEOUT(timeout, msec_timeout, READTIMESTR);
	/* Length is in units of characters, MAX_STRLEN and allocation unit in stp is bytes. Compute the worst case need in bytes.
	 * Worst case, every Unicode char is 4 bytes
	 */
	b_length = (!IS_UTF_CHSET(io_curr_device.in->ichset)) ? length : (length * 4);
	if (0 >= length)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RDFLTOOSHORT);
	/* This check is more useful in "M" mode. For UTF-8 mode, checks have to be done while reading */
	if (MAX_STRLEN < length)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RDFLTOOLONG);
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free <= stringpool.top);
	v->mvtype = MV_STR;
	v->str.len = 0;		/* Nothing kept from any old value */
	ENSURE_STP_FREE_SPACE(b_length + ESC_LEN);
	v->str.addr = (char *)stringpool.free;
	active_device = io_curr_device.in;
	stat = (io_curr_device.in->disp_ptr->readfl)(v, length, msec_timeout);
	if (IS_AT_END_OF_STRINGPOOL(v->str.addr, 0))
		stringpool.free += v->str.len;	/* see iott_readfl */
	assert((int4)v->str.len <= b_length);
	assert(stringpool.free <= stringpool.top);
#	if defined(KEEP_zOS_EBCDIC)
	if (DEFAULT_CODE_SET != active_device->in_code_set)
	{
		cnt = insize = outsize = v->str.len;
		assert(stringpool.free >= stringpool.base);
		ENSURE_STP_FREE_SPACE(cnt);
		temp_ch = stringpool.free;
		save_ptr = v->str.addr;
		start_ptr = (char *)temp_ch;
		stringpool.free += cnt;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.free <= stringpool.top);
		ICONVERT(active_device->input_conv_cd, (unsigned char **)&(v->str.addr), &insize, &temp_ch, &outsize);
		v->str.addr = start_ptr;
	}
#	endif
	active_device = 0;
	return ((NO_M_TIMEOUT != msec_timeout) ? stat : FALSE);
}
