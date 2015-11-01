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

GBLREF bool 	caller_id_flag;
GBLREF uint4	process_id;

void jnl_send_oper(jnl_private_control *jpc, uint4 status)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_buffer_ptr_t	jb;
	uint4			now_writer, fsync_pid;
	int4			io_in_prog, fsync_in_prog;

	error_def(ERR_CALLERID);
	error_def(ERR_JNLBUFINFO);
	error_def(ERR_JNLSENDOPER);

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
	csd = csa->hdr;
	jb = jpc->jnl_buff;
	if (0 != status)
	{
		if (SS_NORMAL != jpc->status)
		{
			if (SS_NORMAL != jpc->status2)
			{
				send_msg(VARLSTCNT(14)
						ERR_JNLSENDOPER, 5, process_id, status, jpc->status, jpc->status2, jb->iosb.cond,
						status, 2, JNL_LEN_STR(csd), jpc->status, 0, jpc->status2);
			} else
				send_msg(VARLSTCNT(12)
						ERR_JNLSENDOPER, 5, process_id, status, jpc->status, jpc->status2, jb->iosb.cond,
						status, 2, JNL_LEN_STR(csd), jpc->status);
		} else
			send_msg(VARLSTCNT(11) ERR_JNLSENDOPER, 5, process_id, status, jpc->status, jpc->status2, jb->iosb.cond,
					status, 2, JNL_LEN_STR(csd));
	}
	jpc->status = SS_NORMAL;
	jpc->status2 = SS_NORMAL;
	UNIX_ONLY(
		io_in_prog = (jb->io_in_prog_latch.latch_pid ? TRUE : FALSE);
		now_writer = jb->io_in_prog_latch.latch_pid;
	)
	VMS_ONLY(
		io_in_prog = jb->io_in_prog;
		now_writer = jb->now_writer;
	)
	fsync_in_prog = jb->fsync_in_prog_latch.latch_pid ? TRUE : FALSE;
	fsync_pid     = jb->fsync_in_prog_latch.latch_pid;
	/* note: the alignment of the parameters below is modelled on the alignment defined for JNLBUFINFO in merrors.msg */
	send_msg(VARLSTCNT(17) ERR_JNLBUFINFO, 15, process_id,
			jb->dsk,      jb->free,     jb->bytcnt,  io_in_prog,     fsync_in_prog,
			jb->dskaddr,  jb->freeaddr, jb->qiocnt,  now_writer,     fsync_pid,
			jb->filesize, jb->errcnt,   jb->wrtsize, jb->fsync_dskaddr);
	caller_id_flag = TRUE;
}
