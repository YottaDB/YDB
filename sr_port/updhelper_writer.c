/****************************************************************
 *								*
 *	Copyright 2005, 2007 Fidelity Information Services, Inc.	*
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
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

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
#ifdef UNIX
#include "gtmio.h"
#endif

#ifdef VMS
#include <ssdef.h>
#include <fab.h>
#include <rms.h>
#include <iodef.h>
#include <secdef.h>
#include <psldef.h>
#include <lckdef.h>
#include <syidef.h>
#include <xab.h>
#include <prtdef.h>
#endif
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

#define UPDHELPER_SLEEP 30
#define THRESHOLD_FOR_PAUSE 10

GBLREF	void			(*call_on_signal)();
GBLREF	recvpool_addrs		recvpool;
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	uint4			process_id;
GBLREF	int			updhelper_log_fd;
GBLREF	FILE			*updhelper_log_fp;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

int updhelper_writer(void)
{
	uint4			pre_read_offset;
	int			lcnt;
	int4			dummy_errno;
	gd_region		*reg, *r_top;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	boolean_t		flushed;

	call_on_signal = updhelper_writer_sigstop;
	updhelper_init(UPD_HELPER_WRITER);
	repl_log(updhelper_log_fp, TRUE, TRUE, "Helper writer started. PID %d [0x%X]\n", process_id, process_id);
	for (lcnt = 0; (NO_SHUTDOWN == helper_entry->helper_shutdown); )
	{
		flushed = FALSE;
		for (reg = gd_header->regions, r_top = reg + gd_header->n_regions; reg < r_top; reg++)
		{
			assert(reg->open); /* we called region_init() in the initialization code */
			csa = &FILE_INFO(reg)->s_addrs;
			cnl = csa->nl;
			csd = csa->hdr;
			if (reg->open && !reg->read_only &&
				(cnl->wcs_active_lvl >= csd->flush_trigger * csd->writer_trigger_factor / 100.0))
			{
				TP_CHANGE_REG(reg); /* for jnl_ensure_open() */
				JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, dummy_errno);
				flushed = TRUE;
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
