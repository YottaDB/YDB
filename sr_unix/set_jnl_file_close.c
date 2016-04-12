/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gtmmsg.h"
#include "wcs_flu.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF  sgmnt_addrs             *cs_addrs;

uint4	set_jnl_file_close(set_jnl_file_close_opcode_t set_jnl_file_close_opcode)
{
	uint4 		jnl_status = 0;

	cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
	jnl_status = jnl_ensure_open();
	if (0 == jnl_status)
	{
		if (0 == cs_addrs->jnl->pini_addr)
			jnl_put_jrt_pini(cs_addrs);
		wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
		jnl_put_jrt_pfin(cs_addrs);
		jnl_file_close(gv_cur_region, TRUE, TRUE);
	} else
		gtm_putmsg(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_addrs->hdr),
			DB_LEN_STR(gv_cur_region));
	return jnl_status;
}
