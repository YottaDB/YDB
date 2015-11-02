/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc	*
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
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */
#include "jnl_typedef.h"

GBLREF  sgmnt_addrs             *cs_addrs;
GBLREF  sgmnt_data_ptr_t	cs_data;
GBLREF  gd_region               *gv_cur_region;
GBLREF  int4			gv_keysize;
GBLREF	recvpool_addrs		recvpool;
GBLREF  boolean_t               repl_allowed;
GBLREF	FILE	 		*updproc_log_fp;
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		secondary_side_std_null_coll;
GBLREF	uint4			process_id;


boolean_t updproc_open_files(gld_dbname_list **gld_db_files, seq_num *start_jnl_seqno)
{
	gld_dbname_list	*curr, *prev;
	sgmnt_addrs	*csa;
	sgmnt_data_ptr_t csd;
	char		*fn;
	sm_uc_ptr_t	gld_fn;
	seq_num		lcl_seqno;
	error_def(ERR_NOREPLCTDREG);
	error_def(ERR_NULLCOLLDIFF);
	gd_region	*reg;

	lcl_seqno = 0;
	VMS_ONLY(jgbl.max_resync_seqno = 0;)
	UNIX_ONLY(jgbl.max_dualsite_resync_seqno = 0;)
	/*
	 *	Open all of the database files
	 */
	/* Unix and VMS have different field names for now, but will both be soon changed to instfilename instead of gtmgbldir */
	UNIX_ONLY(gld_fn = (sm_uc_ptr_t)recvpool.recvpool_ctl->recvpool_id.instfilename;)
	VMS_ONLY(gld_fn = (sm_uc_ptr_t)recvpool.recvpool_ctl->recvpool_id.gtmgbldir;)
	secondary_side_std_null_coll = -1;
	for (curr = *gld_db_files, *gld_db_files = NULL;  NULL != curr;)
	{
		reg = curr->gd;
		fn = (char *)reg->dyn.addr->fname;
		csa = &FILE_INFO(reg)->s_addrs;	/* Work of dbfilopn i.e. assigning file_cntl has been done already in read
								db_files_from_gld module */
		gvcst_init(reg);
		csd = csa->hdr;
		/* Check whether all regions have same null collation order */
		if (secondary_side_std_null_coll != csa->hdr->std_null_coll)
		{
			if (-1 == secondary_side_std_null_coll)
				secondary_side_std_null_coll = csa->hdr->std_null_coll;
			else
				rts_error(VARLSTCNT(1) ERR_NULLCOLLDIFF);
		}
		if (DBKEYSIZE(csa->hdr->max_key_size) > gv_keysize)
			gv_keysize = DBKEYSIZE(csa->hdr->max_key_size);
		SET_CSA_DIR_TREE(csa, reg->max_key_size, reg);
		csa->now_crit = FALSE;
		if (reg->was_open)	 /* Should never happen as only open one at a time, but handle for safety */
		{
			assert(FALSE);
			util_out_print("Error opening database file !AZ", TRUE, fn);
			return FALSE;
		}
		repl_log(updproc_log_fp, TRUE, TRUE, " Process %u Opening File -- %s :: reg_seqno = "INT8_FMT" "INT8_FMTX" \n",
			process_id, fn, INT8_PRINT(csa->hdr->reg_seqno), INT8_PRINTX(csa->hdr->reg_seqno));
		UNIX_ONLY(
			/* The assignment of Seqno needs to be done before checking the state of replication since receiver server
			 * expects the update process to write Seqno in the recvpool before initiating communication with the
			 * source server.
			 */
			if (recvpool.upd_proc_local->updateresync)
				csa->hdr->dualsite_resync_seqno = csa->hdr->reg_seqno;
			if (lcl_seqno < csa->hdr->reg_seqno)
				lcl_seqno = csa->hdr->reg_seqno;
			if (jgbl.max_dualsite_resync_seqno < csa->hdr->dualsite_resync_seqno)
				jgbl.max_dualsite_resync_seqno = csa->hdr->dualsite_resync_seqno;
		)
		VMS_ONLY(
			/* The assignment of Seqno needs to be done before checking the state of replication since receiver
				server expects the update process to write Seqno in the recvpool before initiating
				communication with the source server */
			if (recvpool.upd_proc_local->updateresync)
				csa->hdr->resync_seqno = csa->hdr->reg_seqno;
			if (lcl_seqno < csa->hdr->resync_seqno)
				lcl_seqno = csa->hdr->resync_seqno;
			/* jgbl.max_resync_seqno should be set only after the test for updateresync because resync_seqno gets
			 * modified to reg_seqno in case receiver is started with -updateresync option */
			if (jgbl.max_resync_seqno < csa->hdr->resync_seqno)
				jgbl.max_resync_seqno = csa->hdr->resync_seqno;
		)
		repl_log(updproc_log_fp, TRUE, TRUE, "             -------->  start_jnl_seqno = "INT8_FMT" "INT8_FMTX"\n",
			INT8_PRINT(lcl_seqno), INT8_PRINTX(lcl_seqno));
		if (!REPL_ALLOWED(csd))
		{
			curr = curr->next;
			continue;
		} else if (REPL_ENABLED(csd) && !JNL_ENABLED(csd))
			GTMASSERT;
		else
			repl_allowed = TRUE;
		VMS_ONLY(
			if (recvpool.upd_proc_local->updateresync)
			{
				TP_CHANGE_REG(reg);
				wcs_flu(WCSFLU_FLUSH_HDR);
			}
		)
		prev = curr;
		curr = curr->next;
		prev->next = *gld_db_files;
		*gld_db_files = prev;
	}
	if (NULL == *gld_db_files)
		gtm_putmsg(VARLSTCNT(3) ERR_NOREPLCTDREG, 1, gld_fn);
	*start_jnl_seqno = lcl_seqno;
	return TRUE;
}
