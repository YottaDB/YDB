/****************************************************************
 *								*
 * Copyright (c) 2005-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"
#include "gtm_time.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_inet.h"

#include <sys/mman.h>
#include <errno.h>

#include "cdb_sc.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "cli.h"
#include "error.h"
#include "repl_dbg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_shutdcode.h"
#include "repl_sp.h"
#include "jnl_write.h"
#include "gtmio.h"
#include "interlock.h"
#include "wcs_flu.h"
#include "wcs_mm_recover.h"
#include "wcs_timer_start.h"

#include "ast.h"
#include "util.h"
#include "op.h"
#include "targ_alloc.h"
#include "dpgbldir.h"
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "tp_change_reg.h"
#include "repl_log.h"
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */
#include "memcoherency.h"
#include "change_reg.h"
#include "wcs_backoff.h"
#include "wcs_wt.h"

#define UPDHELPER_SLEEP 30
#define UPDHELPER_EARLY_EPOCH 5
#define THRESHOLD_FOR_PAUSE 10

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	void			(*call_on_signal)();
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	uint4			process_id;
GBLREF	int			updhelper_log_fd;
GBLREF	FILE			*updhelper_log_fp;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	int4			strm_index;
#ifdef DEBUG
GBLREF	sgmnt_addrs		*reorg_encrypt_restart_csa;
#endif

int updhelper_writer(void)
{
	uint4			pre_read_offset;
	int			lcnt;
	int4			dummy_errno, buffs_per_flush, flush_target;
	gd_region		*reg, *r_top;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	boolean_t		flushed;

	call_on_signal = updhelper_writer_sigstop;
	updhelper_init(UPD_HELPER_WRITER);
	repl_log(updhelper_log_fp, TRUE, TRUE, "Helper writer started. PID %d [0x%X]\n", process_id, process_id);
	/* Since we might write epoch records (through wcs_flu/wcs_clean_dbsync), make sure "strm_end_seqno"
	 * for streams #0 thru #15 are written out to jnl file header in "jnl_write_epoch_rec".
	 * Set global variable "strm_index" to 0 to ensure this happens. Note that the actual "strm_index" of the
	 * update process corresponding to this helper writer process could be INVALID_SUPPL_STRM or 1,2, etc.
	 * but it is not easy to keep accurate track of that and that is not needed anyways. All we need is for
	 * strm_index to be not INVALID_SUPPL_STRM and a value of 0 achieves that easily.
	 */
	strm_index = 0;
	for (lcnt = 0; (NO_SHUTDOWN == helper_entry->helper_shutdown); )
	{
		flushed = FALSE;
		for (reg = gd_header->regions, r_top = reg + gd_header->n_regions; reg < r_top; reg++)
		{
			assert(reg->open); /* we called region_init() in the initialization code */
			csa = &FILE_INFO(reg)->s_addrs;
			cnl = csa->nl;
			csd = csa->hdr;
			if (reg->open && !reg->read_only)
			{
				TP_CHANGE_REG(reg); /* for jnl_ensure_open() */
				if (dba_mm == REG_ACC_METH(reg))
				{
					/* Handle MM file extensions so that the flush timer can function properly. */
					MM_DBFILEXT_REMAP_IF_NEEDED(csa, reg);
				}
				wcs_timer_start(reg, TRUE);
				if ((cnl->wcs_active_lvl >= csd->flush_trigger * csd->writer_trigger_factor / 100.0)
						&& !FROZEN_CHILLED(csa))
				{
					JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, NULL, FALSE, dummy_errno);
					flushed = TRUE;
				}
				assert(NULL == reorg_encrypt_restart_csa); /* ensure above wcs_wtstart call does not set it */
				if (JNL_ENABLED(csd))
				{
					jpc = csa->jnl;
					jbp = jpc->jnl_buff;
					/* Open the journal so the flush timer can flush journal records. */
					if ((NOJNL == jpc->channel) || JNL_FILE_SWITCHED(jpc))
						ENSURE_JNL_OPEN(csa, reg);
					SET_GBL_JREC_TIME;
					assert(jgbl.gbl_jrec_time);
					if (((jbp->next_epoch_time - UPDHELPER_EARLY_EPOCH) <= jgbl.gbl_jrec_time)
						 && !FROZEN_CHILLED(csa))
					{
						DO_DB_FSYNC_OUT_OF_CRIT_IF_NEEDED(reg, csa, jpc, jbp);
						if (grab_crit_immediate(reg, OK_FOR_WCS_RECOVER_TRUE))
						{
							if ((jbp->next_epoch_time - UPDHELPER_EARLY_EPOCH) <= jgbl.gbl_jrec_time)
							{
								ENSURE_JNL_OPEN(csa, reg);
								wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH
												| WCSFLU_SPEEDUP_NOBEFORE);
								assert(NULL == reorg_encrypt_restart_csa);
								assert(jbp->next_epoch_time > jgbl.gbl_jrec_time);
							}
							rel_crit(reg);
							/* Do equivalent of WCSFLU_SYNC_EPOCH now out of crit */
							DO_DB_FSYNC_OUT_OF_CRIT_IF_NEEDED(reg, csa, jpc, jbp);
						}
					}
				 }
			 }
		}
		if (!flushed)
		{
			if (lcnt++ >= THRESHOLD_FOR_PAUSE)
			{
				SHORT_SLEEP(UPDHELPER_SLEEP);
				lcnt = 0;
			}
		} else
			lcnt = 0;
	}
	updhelper_writer_end();
	return SS_NORMAL;
}
