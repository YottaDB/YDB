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

#include <sys/mman.h>
#include "gtm_unistd.h"
#include "gtm_socket.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_time.h"

#ifdef UNIX
#include "gtm_ipc.h"
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/un.h>
#ifndef __MVS__
#include <sys/param.h>
#endif
#elif defined(VMS)
#include <psldef.h>
#include <descrip.h> /* Required for gtmrecv.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "repl_shutdcode.h"
#include "eintr_wrappers.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_sem.h"
#ifdef VMS
#include "repl_shm.h"
#endif
#include "repl_log.h"
#ifdef UNIX
#include "mutex.h"
#include "anticipatory_freeze.h"
#endif
#include "gtm_event_log.h"
#include "mupip_exit.h"
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "have_crit.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	recvpool_addrs		recvpool;
GBLREF  gld_dbname_list		*upd_db_files;
GBLREF	boolean_t	        pool_init;
GBLREF	jnlpool_addrs	        jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	void                    (*call_on_signal)();
GBLREF	FILE			*updproc_log_fp;

void  updproc_stop(boolean_t exit)
{
	int4		status;
	int		fclose_res, idx, save_errno;
	seq_num		log_seqno, log_seqno1, jnlpool_seqno, jnlpool_strm_seqno[MAX_SUPPL_STRMS];
	sgmnt_addrs	*repl_csa;
	UNIX_ONLY(
		int4	strm_idx;
		DCL_THREADGBL_ACCESS;
	)

	UNIX_ONLY(
		SETUP_THREADGBL_ACCESS;
	)
	call_on_signal = NULL;	/* Don't reenter on error */
	if (pool_init)
	{
		DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
		assert(!repl_csa->hold_onto_crit);
		jnlpool_seqno = jnlpool.jnlpool_ctl->jnl_seqno;
		UNIX_ONLY(
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				jnlpool_strm_seqno[idx] = jnlpool.jnlpool_ctl->strm_seqno[idx];
		)
		log_seqno = recvpool.recvpool_ctl->jnl_seqno;
		log_seqno1 = recvpool.upd_proc_local->read_jnl_seqno;
		UNIX_ONLY(strm_idx = recvpool.gtmrecv_local->strm_index;)
		rel_lock(jnlpool.jnlpool_dummy_reg);
		repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Current Jnlpool Seqno : %llu\n", jnlpool_seqno);
#		ifdef UNIX
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		{
			if (jnlpool_strm_seqno[idx])
				repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Stream # %d : Current Jnlpool Stream Seqno "
					": %llu\n", idx, jnlpool_strm_seqno[idx]);
		}
		if (0 < strm_idx)
			repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Update process has Stream # %d\n", strm_idx);
#		endif
		repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Current Update process Read Seqno : %llu\n", log_seqno1);
		repl_log(updproc_log_fp, TRUE, TRUE, "REPL INFO - Current Receive Pool Seqno : %llu\n", log_seqno);
#		ifdef UNIX
		if (!ANTICIPATORY_FREEZE_AVAILABLE)
		{
			mutex_cleanup(jnlpool.jnlpool_dummy_reg);
			JNLPOOL_SHMDT(status, save_errno);
			if (0 > status)
				repl_log(stderr, TRUE, TRUE, "Error detaching from jnlpool : %s\n", STRERROR(save_errno));
		}
#		elif defined(VMS)
		jnlpool_ctl = jnlpool.jnlpool_ctl = NULL;
		if (SS$_NORMAL != (status = detach_shm(jnlpool.shm_range)))
			repl_log(stderr, TRUE, TRUE, "Error detaching from jnlpool : %s\n", REPL_STR_ERROR);
		if (SS$_NORMAL != (status = signoff_from_gsec(jnlpool.shm_lockid)))
			repl_log(stderr, TRUE, TRUE, "Error dequeueing lock on jnlpool global section : %s\n", REPL_STR_ERROR);
#		else
#		error Unsupported Platform
#		endif
		pool_init = FALSE;
	}
	recvpool.upd_proc_local->upd_proc_shutdown = NORMAL_SHUTDOWN;
	/* On UNIX, the receiver server needs to do a WAITPID on the update process so that the STOPed update process can be
	 * reaped by the OS and don't go into the defunct state. So, do not reset the upd_proc_pid
	 */
	VMS_ONLY(recvpool.upd_proc_local->upd_proc_pid = 0;)
#ifdef UNIX
	SHMDT(recvpool.recvpool_ctl);
#elif defined(VMS)
	if(SS$_NORMAL != (status = detach_shm(recvpool.shm_range)))
		repl_log(stderr, TRUE, TRUE, "Update process could not detach from recvpool : %s\n", REPL_STR_ERROR);
	if (SS$_NORMAL != (status = signoff_from_gsec(recvpool.shm_lockid)))
		repl_log(stderr, TRUE, TRUE, "Error dequeueing lock on recvpool global section : %s\n", REPL_STR_ERROR);
#else
#error Unsupported Platform
#endif
	recvpool.recvpool_ctl = NULL;
	gtm_event_log_close();
	if (exit)
		mupip_exit(SS_NORMAL);
	return;
}

void updproc_sigstop(void)
{
	updproc_stop(FALSE);
	return;
}

void updproc_end(void)
{
	updproc_stop(TRUE);
	return;
}
