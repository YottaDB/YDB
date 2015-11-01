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

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "stringpool.h"

GBLREF spdesc stringpool;

void view_jnlfile(mval *dst, gd_region *reg)
{
	sm_uc_ptr_t	jnl_name_addr;
	sgmnt_addrs	*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	switch(csa->hdr->jnl_state)
	{
	case jnl_notallowed:
		dst->str.len = 0;
		break;
	case jnl_open:
		if (NOJNL != csa->jnl->channel)
		{
			jnl_name_addr = JNL_NAME_EXP_PTR(csa->jnl->jnl_buff);
			dst->str.len = strlen((sm_c_ptr_t)jnl_name_addr);
			break;
		}
	/* WARNING - fall through */
	case jnl_closed:
		jnl_name_addr = csa->hdr->jnl_file_name;
		dst->str.len = csa->hdr->jnl_file_len;
		break;
	default:
		GTMASSERT;
		break;
	}
	if (0 != dst->str.len)
	{	/* this is basically s2pool replicated because an mstr can only hold a 4 byte address */
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.free <= stringpool.top);
		if ((int)dst->str.len > stringpool.top - stringpool.free)
			stp_gcol(dst->str.len);
		memcpy(stringpool.free, jnl_name_addr, dst->str.len);
		dst->str.addr = (char *)stringpool.free;
		stringpool.free += dst->str.len;
		assert(stringpool.free >= stringpool.base);
		assert(stringpool.free <= stringpool.top);
	}
}
