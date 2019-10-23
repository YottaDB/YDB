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

GBLREF boolean_t	caller_id_flag;
GBLREF uint4		process_id;

error_def(ERR_CALLERID);
error_def(ERR_JNLBUFINFO);
error_def(ERR_JNLNOCREATE);
error_def(ERR_JNLPVTINFO);
error_def(ERR_JNLSENDOPER);

void jnl_send_oper(jnl_private_control *jpc, uint4 status)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_buffer_ptr_t	jb;
	uint4			now_writer, fsync_pid;
	int4			io_in_prog, fsync_in_prog;
	boolean_t		ok_to_log;	/* TRUE except when we avoid flooding operator log due to ENOSPC error */

	switch(jpc->region->dyn.addr->acc_meth)
	{
	case dba_mm:
	case dba_bg:
		csa = &FILE_INFO(jpc->region)->s_addrs;
		break;
	default:
		assertpro(FALSE && jpc->region->dyn.addr->acc_meth);
	}
	csd = csa->hdr;
	jb = jpc->jnl_buff;
	assert((ENOSPC != jpc->status) || jb->enospc_errcnt || WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC));
	assert((SS_NORMAL == jpc->status) || (ENOSPC == jpc->status) || !jb->enospc_errcnt
		|| (jb->enospc_errcnt && (ERR_JNLNOCREATE == jpc->status)));
	ok_to_log = (jb->enospc_errcnt ?  (1 == (jb->enospc_errcnt % ENOSPC_LOGGING_PERIOD)) : TRUE);

	caller_id_flag = FALSE;
	if (ok_to_log)
	{
		SEND_CALLERID("jnl_send_oper()");
		if (0 != status)
		{
			if (NULL != jpc->err_str)
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(11) ERR_JNLSENDOPER, 5, process_id, status, jpc->status,
					jpc->status2, jb->iosb.cond, ERR_TEXT, 2, LEN_AND_STR(jpc->err_str));
			else if (SS_NORMAL != jpc->status2)
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(14)
					ERR_JNLSENDOPER, 5, process_id, status, jpc->status, jpc->status2, jb->iosb.cond,
					status, 2, JNL_LEN_STR(csd), jpc->status, 0, jpc->status2);
			else if (SS_NORMAL != jpc->status)
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(12)
					ERR_JNLSENDOPER, 5, process_id, status, jpc->status, jpc->status2, jb->iosb.cond,
					status, 2, JNL_LEN_STR(csd), jpc->status);
			else
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(11) ERR_JNLSENDOPER, 5, process_id, status, jpc->status,
					jpc->status2, jb->iosb.cond, status, 2, JNL_LEN_STR(csd));
		}
	}
	jpc->status = SS_NORMAL;
	jpc->status2 = SS_NORMAL;
	io_in_prog = (jb->io_in_prog_latch.u.parts.latch_pid ? TRUE : FALSE);
	now_writer = jb->io_in_prog_latch.u.parts.latch_pid;
	fsync_in_prog = jb->fsync_in_prog_latch.u.parts.latch_pid ? TRUE : FALSE;
	fsync_pid     = jb->fsync_in_prog_latch.u.parts.latch_pid;
	if (ok_to_log)
	{
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(23) ERR_JNLBUFINFO, 21, process_id,
			jb->dsk, jb->free, jb->bytcnt, io_in_prog, fsync_in_prog, jb->dskaddr, jb->freeaddr, jb->qiocnt,
			now_writer, fsync_pid, jb->filesize, jb->cycle, jb->errcnt, jb->wrtsize, jb->fsync_dskaddr,
			jb->rsrv_free, jb->rsrv_freeaddr, jb->phase2_commit_index1, jb->phase2_commit_index2, jb->next_align_addr);
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(10) ERR_JNLPVTINFO, 8, process_id, jpc->cycle, jpc->fd_mismatch,
			jpc->channel, jpc->sync_io, jpc->pini_addr, jpc->qio_active, jpc->old_channel);
	}
	caller_id_flag = TRUE;
}
