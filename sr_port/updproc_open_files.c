/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
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
#include "error.h"
#include "repl_msg.h"
#include "gtmsource.h"

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
#include "util.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "targ_alloc.h"
#include "dpgbldir.h"
#include "read_db_files_from_gld.h"
#include "wcs_flu.h"
#include "updproc.h"
#include "repl_log.h"
#include "gtmmsg.h"	/* for "gtm_putmsg" prototype */
#include "jnl_typedef.h"

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	int4			gv_keysize;
GBLREF	recvpool_addrs		recvpool;
GBLREF	FILE			*updproc_log_fp;
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			process_id;
#ifndef UNIX
GBLREF	boolean_t		secondary_side_std_null_coll;
#endif
GBLREF	jnlpool_addrs		jnlpool;
#ifdef DEBUG
GBLREF	boolean_t		pool_init;
#endif

error_def(ERR_NOREPLCTDREG);
error_def(ERR_NULLCOLLDIFF);

boolean_t updproc_open_files(gld_dbname_list **gld_db_files, seq_num *start_jnl_seqno)
{
	boolean_t		this_side_std_null_coll;
	char			*fn;
	gd_region		*reg;
	gld_dbname_list		*curr, *prev;
	seq_num			lcl_seqno;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		gld_fn;

	/* Open all of the database files */
	this_side_std_null_coll = -1;
	for (curr = *gld_db_files, *gld_db_files = NULL;  NULL != curr;)
	{
		reg = curr->gd;
		fn = (char *)reg->dyn.addr->fname;
		csa = &FILE_INFO(reg)->s_addrs;	/* Work of dbfilopn - assigning file_cntl already done db_files_from_gld */
		gvcst_init(reg);
		csd = csa->hdr;
		/* Check whether all regions have same null collation order */
		if (this_side_std_null_coll != csd->std_null_coll)
		{
			if (-1 == this_side_std_null_coll)
				this_side_std_null_coll = csd->std_null_coll;
			else
				rts_error(VARLSTCNT(1) ERR_NULLCOLLDIFF);
		}
		if (DBKEYSIZE(csd->max_key_size) > gv_keysize)
			gv_keysize = DBKEYSIZE(csd->max_key_size);
		SET_CSA_DIR_TREE(csa, reg->max_key_size, reg);
		assert(!csa->now_crit);
		if (reg->was_open)	/* Should never happen as only open one at a time, but handle for safety */
		{
			assert(FALSE);
			util_out_print("Error opening database file !AZ", TRUE, fn);
			return FALSE;
		}
		repl_log(updproc_log_fp, TRUE, TRUE, "Process %u Opening File -- %s :: reg_seqno = "INT8_FMT" "INT8_FMTX"\n",
			process_id, fn, INT8_PRINT(csd->reg_seqno), INT8_PRINTX(csd->reg_seqno));
		if (!REPL_ALLOWED(csd))
		{
			curr = curr->next;
			continue;
		} else if (REPL_ENABLED(csd) && !JNL_ENABLED(csd))
			GTMASSERT;
#		ifdef VMS
		if (recvpool.upd_proc_local->updateresync)
		{
			TP_CHANGE_REG(reg);
			wcs_flu(WCSFLU_FLUSH_HDR);
		}
#		endif
		prev = curr;
		curr = curr->next;
		prev->next = *gld_db_files;
		*gld_db_files = prev;
	}
	assert((FALSE == this_side_std_null_coll) || (TRUE == this_side_std_null_coll));
	if (NULL == *gld_db_files)
	{	/* No replicated region found. Don't know how this could happen for the update process as the source server
		 * which starts BEFORE the update process should have detected this and issued the NOREPLCTDREG error. So,
		 * assert in DBG and issue error in PRO. The error in PRO should be an rts_error as without any replicated
		 * region, there is no reason for the update process to continue.
		 */
		assert(FALSE);
		/* Unix and VMS have different field names */
		UNIX_ONLY(gld_fn = (sm_uc_ptr_t)recvpool.recvpool_ctl->recvpool_id.instfilename;)
		VMS_ONLY(gld_fn = (sm_uc_ptr_t)recvpool.recvpool_ctl->recvpool_id.gtmgbldir;)
		rts_error(VARLSTCNT(5) ERR_NOREPLCTDREG, 3,
			   LEN_AND_LIT(UNIX_ONLY("instance file") VMS_ONLY("global directory")), gld_fn);
	}
	/* Now that all the databases are opened, compute the MAX region sequence number across all regions */
	assert((NULL != jnlpool.jnlpool_ctl) && pool_init); /* jnlpool_init should have already been done */
	VMS_ONLY(jgbl.max_resync_seqno = 0;)
	lcl_seqno = 0;
	/* Before looking at the region sequence numbers (to compute the MAX of them), we need to ensure that NO concurrent
	 * online rollback happens. So, grab the journal pool lock as rollback needs it. The reason we do it in two separate
	 * loops instead of one is because we don't want to do gvcst_init while holding the journal pool lock as the former
	 * acquires ftok and access control semaphores and holding the journal pool lock is a potential recipe for deadlocks
	 */
	UNIX_ONLY(grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY));
	for (curr = *gld_db_files; NULL != curr; curr = curr->next)
	{
		reg = curr->gd;
		csa = &FILE_INFO(reg)->s_addrs;	/* Work of dbfilopn - assigning file_cntl already done db_files_from_gld */
		csd = csa->hdr;
#		ifdef UNIX
		if (lcl_seqno < csd->reg_seqno)
			lcl_seqno = csd->reg_seqno;
#		else
		if (recvpool.upd_proc_local->updateresync)
			csd->resync_seqno = csd->reg_seqno;
		if (lcl_seqno < csd->resync_seqno)
			lcl_seqno = csd->resync_seqno;
		/* jgbl.max_resync_seqno should be set only after the test for updateresync because resync_seqno gets
		 * modified to reg_seqno in case receiver is started with -updateresync option */
		if (jgbl.max_resync_seqno < csd->resync_seqno)
			jgbl.max_resync_seqno = csd->resync_seqno;
#		endif
	}
	repl_log(updproc_log_fp, TRUE, TRUE, "             -------->  start_jnl_seqno = "INT8_FMT" "INT8_FMTX"\n",
			INT8_PRINT(lcl_seqno), INT8_PRINTX(lcl_seqno));
	UNIX_ONLY(rel_lock(jnlpool.jnlpool_dummy_reg));
	assert(0 != lcl_seqno);
	*start_jnl_seqno = lcl_seqno;
	UNIX_ONLY(recvpool.recvpool_ctl->this_side.is_std_null_coll = this_side_std_null_coll;)
	VMS_ONLY(secondary_side_std_null_coll = this_side_std_null_coll;)
	return TRUE;
}
