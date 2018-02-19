/****************************************************************
 *								*
 * Copyright (c) 2006-2017 Fidelity National Information	*
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
#include "gtm_time.h"
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "jnl.h"
#include "change_reg.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	uint4			process_id;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

error_def(ERR_JNLEXTEND);

/* This function is called primarily to append a new histinfo record to the replication instance file by one of the following
 *	1) MUPIP REPLIC -SOURCE -START -ROOTPRIMARY command (after forking the child source server) if it created the journal pool.
 *	2) MUPIP REPLIC -SOURCE -ACTIVATE -ROOTPRIMARY command if this is a propagating primary to root primary transition.
 * In addition, this function also initializes the "lms_group_info" field in the instance file (from the "inst_info" field)
 *	if the current value is NULL.
 */
void	gtmsource_rootprimary_init(seq_num start_seqno)
{
	unix_db_info		*udi;
	repl_histinfo		histinfo;
	boolean_t		was_crit, switch_jnl;
	gd_region		*reg, *region_top;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	uint4			jnl_status;

	assert(NULL != jnlpool);
	udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
	assert(NULL != jnlpool->repl_inst_filehdr);
	/* Update journal pool fields to reflect this is a root primary startup and updates are enabled */
	assert(!udi->s_addrs.hold_onto_crit || jgbl.onlnrlbk);
	was_crit = udi->s_addrs.now_crit;
	if (!was_crit)
		grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
	jnlpool->repl_inst_filehdr->root_primary_cycle++;
	/* If this instance is transitioning from a non-rootprimary to rootprimary, switch journal files.
	 * This helps with maintaining accurate value of csd->zqgblmod_tn when the former primary connects
	 * to the current primary through a fetchresync-rollback or receiver-server-autorollback..
	 */
	switch_jnl = (!jnlpool->repl_inst_filehdr->was_rootprimary && (0 < jnlpool->repl_inst_filehdr->num_histinfo));
	jnlpool->repl_inst_filehdr->was_rootprimary = TRUE;
	assert(start_seqno >= jnlpool->jnlpool_ctl->start_jnl_seqno);
	assert(start_seqno == jnlpool->jnlpool_ctl->jnl_seqno);
	jnlpool->repl_inst_filehdr->jnl_seqno = start_seqno;
	assert(jgbl.onlnrlbk || jnlpool->jnlpool_ctl->upd_disabled);
	if (!jgbl.onlnrlbk)
		jnlpool->jnlpool_ctl->upd_disabled = FALSE;
	if (IS_REPL_INST_UUID_NULL(jnlpool->repl_inst_filehdr->lms_group_info))
	{	/* This is the first time this instance is being brought up either as a root primary or as a propagating
		 * primary. Initialize the "lms_group_info" fields in the instance file header in journal pool shared memory.
		 * They will be flushed to the instance file as part of the "repl_inst_histinfo_add -> repl_inst_flush_filehdr"
		 * function invocation below.
		 */
		assert('\0' == jnlpool->repl_inst_filehdr->lms_group_info.created_nodename[0]);
		assert('\0' == jnlpool->repl_inst_filehdr->lms_group_info.this_instname[0]);
		assert(!jnlpool->repl_inst_filehdr->lms_group_info.creator_pid);
		jnlpool->repl_inst_filehdr->lms_group_info = jnlpool->repl_inst_filehdr->inst_info;
		assert('\0' != jnlpool->repl_inst_filehdr->lms_group_info.created_nodename[0]);
		DBG_CHECK_CREATED_NODENAME(jnlpool->repl_inst_filehdr->lms_group_info.created_nodename);
		assert('\0' != jnlpool->repl_inst_filehdr->lms_group_info.this_instname[0]);
		assert(jnlpool->repl_inst_filehdr->lms_group_info.created_time);
		assert(jnlpool->repl_inst_filehdr->lms_group_info.creator_pid);
	}
	/* Initialize histinfo fields */
	memcpy(histinfo.root_primary_instname, jnlpool->repl_inst_filehdr->inst_info.this_instname, MAX_INSTNAME_LEN - 1);
	histinfo.root_primary_instname[MAX_INSTNAME_LEN - 1] = '\0';
	assert('\0' != histinfo.root_primary_instname[0]);
	histinfo.start_seqno = start_seqno;
	assert(jnlpool->jnlpool_ctl->strm_seqno[0] == jnlpool->repl_inst_filehdr->strm_seqno[0]);
	assert(jnlpool->repl_inst_filehdr->is_supplementary || (0 == jnlpool->jnlpool_ctl->strm_seqno[0]));
	histinfo.strm_seqno = (!jnlpool->repl_inst_filehdr->is_supplementary) ? 0 : jnlpool->jnlpool_ctl->strm_seqno[0];
	histinfo.root_primary_cycle = jnlpool->repl_inst_filehdr->root_primary_cycle;
	assert(process_id == getpid());
	histinfo.creator_pid = process_id;
	JNL_SHORT_TIME(histinfo.created_time);
	histinfo.strm_index = 0;
	histinfo.history_type = HISTINFO_TYPE_NORMAL;
	NULL_INITIALIZE_REPL_INST_UUID(histinfo.lms_group);
	/* The following fields will be initialized in the "repl_inst_histinfo_add" function call below.
	 *	histinfo.histinfo_num
	 *	histinfo.prev_histinfo_num
	 *	histinfo.last_histinfo_num[]
	 */
	/* Add the histinfo record to the instance file and flush the changes in the journal pool to the file header */
	repl_inst_histinfo_add(&histinfo);
	if (!was_crit)
		rel_lock(jnlpool->jnlpool_dummy_reg);
	if (switch_jnl)
	{
		SET_GBL_JREC_TIME; /* jnl_ensure_open/jnl_file_extend and its callees assume jgbl.gbl_jrec_time is set */
		for (reg = gd_header->regions, region_top = gd_header->regions + gd_header->n_regions; reg < region_top; reg++)
		{
			gv_cur_region = reg;
			change_reg();		/* sets cs_addrs/cs_data (needed by jnl_ensure_open) */
			if (!JNL_ENABLED(cs_addrs))
				continue;
			grab_crit(gv_cur_region);
			jpc = cs_addrs->jnl;
			/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl
			 * records. This needs to be done BEFORE the jnl_ensure_open as that could write journal records
			 * (if it decides to switch to a new journal file)
			 */
			jbp = jpc->jnl_buff;
			ADJUST_GBL_JREC_TIME(jgbl, jbp);
			jnl_status = jnl_ensure_open(gv_cur_region, cs_addrs);
			if (0 == jnl_status)
			{
				if (EXIT_ERR == SWITCH_JNL_FILE(jpc))
					rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLEXTEND, 2, JNL_LEN_STR(cs_data));
			} else
			{
				if (SS_NORMAL != jpc->status)
					rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(cs_data),
							DB_LEN_STR(gv_cur_region), jpc->status);
				else
					rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(cs_data),
							DB_LEN_STR(gv_cur_region));
			}
			rel_crit(gv_cur_region);
		}
	}
}
