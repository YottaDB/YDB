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
#include "iosp.h"
#include "jnl.h"
#include "error.h"
#include "send_msg.h"
#include "caller_id.h"

GBLREF bool caller_id_flag;

void jnl_send_oper4(jnl_private_control *jpc, uint4 status, uint4 status2, int4 cond);

void jnl_send_oper4(jnl_private_control *jpc, uint4 status, uint4 status2, int4 cond)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_buffer_ptr_t	jb;

	error_def(ERR_JNLBUFINFO);

	switch(jpc->region->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		csa = &FILE_INFO(jpc->region)->s_addrs;
		break;
	default:
		GTMASSERT;
	}
	csd = csa->hdr;
	jb = jpc->jnl_buff;
	if (0 != status2 || 0 != status)
	{
		if (0 != status2)
			send_msg(VARLSTCNT(6) status, 2, JNL_LEN_STR(csd), status2, cond);
		else /* 0 != status */
			send_msg(VARLSTCNT(4) status, 2, JNL_LEN_STR(csd));
		send_msg(VARLSTCNT(17) ERR_JNLBUFINFO, 15, jb->dskaddr, jb->freeaddr, jb->dsk, jb->free,
			jb->lastaddr, jb->wrtsize, jb->bytcnt, jb->qiocnt, jb->errcnt,
#ifdef VMS
			jb->io_in_prog, jb->now_writer, jb->filesize,
#elif defined(UNIX)
			(jb->io_in_prog_latch.latch_pid ? TRUE : FALSE), jb->io_in_prog_latch.latch_pid, jb->filesize,
#endif
			(jb->fsync_in_prog_latch.latch_pid ? TRUE : FALSE),
			jb->fsync_in_prog_latch.latch_pid, jb->fsync_dskaddr);
	}
}

void jnl_send_oper(jnl_private_control *jpc, uint4 status)
{
	sgmnt_addrs	*csa;
	uint4		status2;
	int4		cond;

	error_def(ERR_CALLERID);

	caller_id_flag = FALSE;
	SEND_CALLERID("jnl_send_oper()");

	switch(jpc->region->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		csa = &FILE_INFO(jpc->region)->s_addrs;
		break;
	default:
		GTMASSERT;
	}
#ifdef VMS
	cond = jpc->jnl_buff ? (int4)jpc->jnl_buff->iosb.cond : 0;
	if (SS_NORMAL == cond)
		cond = 0;
#else
	cond = 0;
#endif
	status2 = jpc->status;
        jpc->status = 0;
	if ((0 == status2) || (SS_NORMAL == status2) || (status2 == cond))
	{
		status2 = cond;
		cond = 0;
	}
        jnl_send_oper4(jpc, status, status2, cond);
	caller_id_flag = TRUE;
}
