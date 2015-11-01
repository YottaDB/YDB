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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"

GBLREF uint4 process_id;

void jnl_flush(gd_region *reg)
{
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;

	if (!reg || !reg->open)
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	assert(csa->now_crit);
	jpc = csa->jnl;
	if (!JNL_ENABLED(csa->hdr) || (NULL == jpc) || (NOJNL == jpc->channel))
		return;
	jb = jpc->jnl_buff;
	jb->blocked = process_id;
	if (jb->freeaddr != jb->dskaddr && SS_NORMAL != jnl_write_attempt(jpc, jb->freeaddr))
		return;
	assert(jb->dskaddr == jb->freeaddr);
	jb->blocked = 0;
	return;
}
