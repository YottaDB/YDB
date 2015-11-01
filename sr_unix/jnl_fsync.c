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

#include <unistd.h>

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

GBLREF uint4		process_id;

void jnl_fsync(gd_region *reg, uint4 fsync_addr)
{
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	uint4			lcnt, saved_dsk_addr, saved_status;
	sgmnt_addrs		*csa;
	int4			lck_state;
	int			fsync_ret, save_errno;

	error_def(ERR_JNLFSYNCERR);
	error_def(ERR_FSYNCTIMOUT);
	error_def(ERR_TEXT);
	error_def(ERR_JNLFRCDTERM);
	error_def(ERR_JNLFSYNCLSTCK);

	csa = &FILE_INFO(reg)->s_addrs;
	jpc = csa->jnl;
	jb  = jpc->jnl_buff;

	if ((NOJNL != jpc->channel) && !JNL_FILE_SWITCHED(jpc->region))
	{
		for (lcnt = 1; fsync_addr > jb->fsync_dskaddr && !JNL_FILE_SWITCHED(jpc->region); lcnt++)
		{
			if (MAX_FSYNC_WAIT_CNT == lcnt / 2)	/* half way into max.patience*/
			{
				saved_status = jpc->status;
				jpc->status = 0;
				jnl_send_oper(jpc, ERR_JNLFSYNCLSTCK);
				jpc->status = saved_status ;
			}
			if (MAX_FSYNC_WAIT_CNT == lcnt)	/* tried a long */
			{
				saved_status = jpc->status;
				jpc->status = 0;
				jnl_send_oper(jpc, ERR_JNLFSYNCLSTCK);
				jpc->status = saved_status ;
				send_msg(VARLSTCNT(4) ERR_FSYNCTIMOUT, 2, jpc->region->jnl_file_len,
										jpc->region->jnl_file_name);
				GTMASSERT;
			}
			BG_TRACE_PRO_ANY(csa, n_jnl_fsync_tries);
			if (GET_SWAPLOCK(&jb->fsync_in_prog_latch))
				break;
			wcs_sleep(lcnt);
			performCASLatchCheck(&jb->fsync_in_prog_latch, lcnt);
		}
		if (fsync_addr > jb->fsync_dskaddr && !JNL_FILE_SWITCHED(jpc->region))
		{
			assert(process_id == jb->fsync_in_prog_latch.latch_pid);  /* assert we have the lock */
			saved_dsk_addr = jb->dskaddr;
			if (jpc->sync_io)
			{
				/* We need to maintain the fsync control fields irrespective of the type of IO, because we might
				 * switch between these at any time.
				 */
				jb->fsync_dskaddr = saved_dsk_addr;
			} else
			{
				GTM_FSYNC(jpc->channel, fsync_ret);
				if (-1 == fsync_ret)
				{
					save_errno = errno;
					assert(FALSE);
					send_msg(VARLSTCNT(9) ERR_JNLFSYNCERR, 2,
						JNL_LEN_STR(jpc->region),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), save_errno);
					rts_error(VARLSTCNT(9) ERR_JNLFSYNCERR, 2,
						JNL_LEN_STR(jpc->region),
						ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), save_errno);
				} else
				{
					jb->fsync_dskaddr = saved_dsk_addr;
					BG_TRACE_PRO_ANY(csa, n_jnl_fsyncs);
				}
			}
		}
		if (process_id == jb->fsync_in_prog_latch.latch_pid)
			RELEASE_SWAPLOCK(&jb->fsync_in_prog_latch);
	}
	return;
}
