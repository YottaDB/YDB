/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#ifdef DEBUG
#include "have_crit.h"		/* for the TPNOTACID_CHECK macro */
#endif

GBLREF uint4		dollar_trestart;
GBLREF io_pair		io_curr_device;
GBLREF io_desc		*active_device;
GBLREF spdesc		stringpool;

error_def(ERR_TEXT);

#define READTIMESTR "READ time too long"

int op_read(mval *v, int4 timeout)
{
	int stat;
	mval val;
	size_t cnt, insize, outsize;
	unsigned char *temp_ch;
	char *start_ptr, *save_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (timeout < 0)
		timeout = 0;
	else if (TREF(tpnotacidtime) < timeout)
		TPNOTACID_CHECK(READTIMESTR);
	active_device = io_curr_device.in;
	v->mvtype = MV_STR;
	v->str.len = 0;
	stat = (io_curr_device.in->disp_ptr->read)(v, timeout);
	if (stringpool.free == (unsigned char *)v->str.addr)		/* BYPASSOK */
		stringpool.free += v->str.len;	/* see UNIX iott_readfl */
	assert(stringpool.free <= stringpool.top);
#	ifdef KEEP_zOS_EBCDIC
	if (DEFAULT_CODE_SET != io_curr_device.in->in_code_set)
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
		ICONVERT(io_curr_device.in->input_conv_cd, (unsigned char **)&v->str.addr, &insize, &temp_ch, &outsize);
		v->str.addr = start_ptr;
	}
#	endif
	active_device = 0;
	if (NO_M_TIMEOUT != timeout)
		return(stat);
	return FALSE;
}
