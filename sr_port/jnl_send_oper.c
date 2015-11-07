/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "wbox_test_init.h"

#define	ENOSPC_LOGGING_PERIOD	100	/* every 100th ENOSPC error is logged to avoid flooding the operator log */

GBLREF bool 	caller_id_flag;
GBLREF uint4	process_id;

void jnl_send_oper(jnl_private_control *jpc, uint4 status)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_buffer_ptr_t	jb;
	uint4			now_writer, fsync_pid;
	int4			io_in_prog, fsync_in_prog;
	boolean_t		ok_to_log;	/* TRUE except when we avoid flooding operator log due to ENOSPC error */

	error_def(ERR_CALLERID);
	error_def(ERR_JNLBUFINFO);
	error_def(ERR_JNLPVTINFO);
	error_def(ERR_JNLSENDOPER);

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
	UNIX_ONLY(assert((ENOSPC != jpc->status) || jb->enospc_errcnt || WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC)));
	UNIX_ONLY(assert((SS_NORMAL == jpc->status) || (ENOSPC == jpc->status) || !jb->enospc_errcnt));
	VMS_ONLY(assert(!jb->enospc_errcnt));	/* currently not updated in VMS, so should be 0 */
	ok_to_log = (jb->enospc_errcnt ?  (1 == (jb->enospc_errcnt % ENOSPC_LOGGING_PERIOD)) : TRUE);

	caller_id_flag = FALSE;
	if (ok_to_log)
	{
		SEND_CALLERID("jnl_send_oper()");
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
				send_msg(VARLSTCNT(11) ERR_JNLSENDOPER, 5, process_id, status, jpc->status, jpc->status2,
					jb->iosb.cond, status, 2, JNL_LEN_STR(csd));
		}
	}
	jpc->status = SS_NORMAL;
	jpc->status2 = SS_NORMAL;
	UNIX_ONLY(
		io_in_prog = (jb->io_in_prog_latch.u.parts.latch_pid ? TRUE : FALSE);
		now_writer = jb->io_in_prog_latch.u.parts.latch_pid;
	)
	VMS_ONLY(
		io_in_prog = jb->io_in_prog;
		now_writer = jb->now_writer;
	)
	fsync_in_prog = jb->fsync_in_prog_latch.u.parts.latch_pid ? TRUE : FALSE;
	fsync_pid     = jb->fsync_in_prog_latch.u.parts.latch_pid;
	/* note: the alignment of the parameters below is modelled on the alignment defined for JNLBUFINFO in merrors.msg */
	if (ok_to_log)
	{
		send_msg(VARLSTCNT(18) ERR_JNLBUFINFO, 16, process_id,
				jb->dsk,      jb->free,     jb->bytcnt,  io_in_prog,  fsync_in_prog,
				jb->dskaddr,  jb->freeaddr, jb->qiocnt,  now_writer,  fsync_pid,
				jb->filesize, jb->cycle,    jb->errcnt,  jb->wrtsize, jb->fsync_dskaddr);
		send_msg(VARLSTCNT(10) ERR_JNLPVTINFO, 8, process_id,
				jpc->cycle,     jpc->fd_mismatch, jpc->channel,    jpc->sync_io,
				jpc->pini_addr, jpc->qio_active,  jpc->old_channel);
	}
	caller_id_flag = TRUE;
}
