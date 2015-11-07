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

#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "aswp.h"
#include "interlock.h"
#include "sleep_cnt.h"
#include "eintr_wrappers.h"
#include "performcaslatchcheck.h"
#include "send_msg.h"
#include "wcs_sleep.h"
#include "is_file_identical.h"
#include "gtm_string.h"
#include "gtm_c_stack_trace.h"
#include "gtmsecshr.h"		/* for continue_proc */
#include "anticipatory_freeze.h"
#include "wbox_test_init.h"
#ifdef DEBUG
#include "gt_timer.h"
#include "gtm_stdio.h"
#include "is_proc_alive.h"
#endif

GBLREF	uint4		process_id;
GBLREF	jnl_gbls_t	jgbl;

error_def(ERR_FSYNCTIMOUT);
error_def(ERR_JNLFRCDTERM);
error_def(ERR_JNLFSYNCERR);
error_def(ERR_JNLFSYNCLSTCK);
error_def(ERR_TEXT);

void jnl_fsync(gd_region *reg, uint4 fsync_addr)
{
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	uint4			lcnt, saved_dsk_addr, saved_status;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	int4			lck_state;
	int			fsync_ret, save_errno;
	DEBUG_ONLY(uint4	onln_rlbk_pid;)

	csa = &FILE_INFO(reg)->s_addrs;
	jpc = csa->jnl;
	jb  = jpc->jnl_buff;
	assert(process_id != CURRENT_JNL_FSYNC_WRITER(jb));
	if ((NOJNL != jpc->channel) && !JNL_FILE_SWITCHED(jpc))
	{
		csd = csa->hdr;
		for (lcnt = 1; fsync_addr > jb->fsync_dskaddr && !JNL_FILE_SWITCHED(jpc); lcnt++)
		{
			if (0 == (lcnt % FSYNC_WAIT_HALF_TIME))
			{
				saved_status = jpc->status;
				jpc->status = SS_NORMAL;
#				ifdef DEBUG
				if (CURRENT_JNL_FSYNC_WRITER(jb))
					GET_C_STACK_FROM_SCRIPT("JNLFSYNCSTUCK_HALF_TIME",
						process_id, CURRENT_JNL_FSYNC_WRITER(jb), lcnt / FSYNC_WAIT_HALF_TIME);
#				endif
				jnl_send_oper(jpc, ERR_JNLFSYNCLSTCK);
				jpc->status = saved_status;
			}
			else if (0 == (lcnt % FSYNC_WAIT_TIME))
			{
				saved_status = jpc->status;
				jpc->status = SS_NORMAL;
				if (CURRENT_JNL_FSYNC_WRITER(jb))
					GET_C_STACK_FROM_SCRIPT("JNLFSYNCSTUCK",
						process_id, CURRENT_JNL_FSYNC_WRITER(jb), lcnt / FSYNC_WAIT_HALF_TIME);
				jnl_send_oper(jpc, ERR_JNLFSYNCLSTCK);
				jpc->status = saved_status;
			}
			BG_TRACE_PRO_ANY(csa, n_jnl_fsync_tries);
			if (GET_SWAPLOCK(&jb->fsync_in_prog_latch))
				break;
			wcs_sleep(lcnt);
			/* trying to wake up the lock holder one iteration before calling c_script */
			if ((lcnt % FSYNC_WAIT_HALF_TIME) == (FSYNC_WAIT_HALF_TIME - 1))
				performCASLatchCheck(&jb->fsync_in_prog_latch, TRUE);
		}
#		ifdef DEBUG
		if (gtm_white_box_test_case_enabled
			&& (WBTEST_EXTEND_JNL_FSYNC == gtm_white_box_test_case_number))
		{
			FPRINTF(stderr, "JNL_FSYNC: will sleep for 40 seconds\n", process_id);
			LONG_SLEEP(40);
			FPRINTF(stderr, "JNL_FSYNC: done sleeping\n", process_id);
			gtm_white_box_test_case_enabled = FALSE;
		}
#		endif
		if (fsync_addr > jb->fsync_dskaddr && !JNL_FILE_SWITCHED(jpc))
		{
			assert(process_id == CURRENT_JNL_FSYNC_WRITER(jb));  /* assert we have the lock */
			saved_dsk_addr = jb->dskaddr;
			if (jpc->sync_io)
			{	/* We need to maintain the fsync control fields irrespective of the type of IO, because we might
				 * switch between these at any time.
				 */
				jb->fsync_dskaddr = saved_dsk_addr;
			} else
			{
				/* If a concurrent online rollback is running, we should never be here since online rollback at the
				 * start flushes all the dirty buffers and ensures that the journal buffers are all synced to disk.
				 * So, there is no need for GT.M processes to reach here with a concurrent online rollback. Assert
				 * to that effect.
				 */
				DEBUG_ONLY(onln_rlbk_pid = csa->nl->onln_rlbk_pid);
				assert(jgbl.onlnrlbk || !onln_rlbk_pid || !is_proc_alive(onln_rlbk_pid, 0)
						|| (onln_rlbk_pid != csa->nl->in_crit));
				GTM_JNL_FSYNC(csa, jpc->channel, fsync_ret);
				GTM_WHITE_BOX_TEST(WBTEST_FSYNC_SYSCALL_FAIL, fsync_ret, -1);
				WBTEST_ASSIGN_ONLY(WBTEST_FSYNC_SYSCALL_FAIL, errno, EIO);
				if (-1 == fsync_ret)
				{
					save_errno = errno;
					assert(WBTEST_ENABLED(WBTEST_FSYNC_SYSCALL_FAIL));
					RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFSYNCERR, 2, JNL_LEN_STR(csd),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), save_errno);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFSYNCERR, 2, JNL_LEN_STR(csd),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), save_errno);
				} else
				{
					jb->fsync_dskaddr = saved_dsk_addr;
					BG_TRACE_PRO_ANY(csa, n_jnl_fsyncs);
				}
			}
		}
		if (process_id == CURRENT_JNL_FSYNC_WRITER(jb))
			RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
	}
	assert(process_id != CURRENT_JNL_FSYNC_WRITER(jb));
	return;
}
