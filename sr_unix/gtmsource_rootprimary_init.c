/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
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

GBLREF	jnlpool_addrs	jnlpool;
GBLREF	uint4		process_id;
GBLREF	jnl_gbls_t	jgbl;


/* This function is called primarily to append a new histinfo record to the replication instance file by one of the following
 *	1) MUPIP REPLIC -SOURCE -START -ROOTPRIMARY command (after forking the child source server) if it created the journal pool.
 *	2) MUPIP REPLIC -SOURCE -ACTIVATE -ROOTPRIMARY command if this is a propagating primary to root primary transition.
 * In addition, this function also initializes the "lms_group_info" field in the instance file (from the "inst_info" field)
 *	if the current value is NULL.
 */
void	gtmsource_rootprimary_init(seq_num start_seqno)
{
	unix_db_info	*udi;
	repl_histinfo	histinfo;
	boolean_t	was_crit;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(NULL != jnlpool.repl_inst_filehdr);
	/* Update journal pool fields to reflect this is a root primary startup and updates are enabled */
	assert(!udi->s_addrs.hold_onto_crit || jgbl.onlnrlbk);
	was_crit = udi->s_addrs.now_crit;
	if (!was_crit)
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
	jnlpool.repl_inst_filehdr->root_primary_cycle++;
	jnlpool.repl_inst_filehdr->was_rootprimary = TRUE;
	assert(start_seqno >= jnlpool.jnlpool_ctl->start_jnl_seqno);
	assert(start_seqno == jnlpool.jnlpool_ctl->jnl_seqno);
	jnlpool.repl_inst_filehdr->jnl_seqno = start_seqno;
	assert(jgbl.onlnrlbk || jnlpool.jnlpool_ctl->upd_disabled);
	if (!jgbl.onlnrlbk)
		jnlpool.jnlpool_ctl->upd_disabled = FALSE;
	if (IS_REPL_INST_UUID_NULL(jnlpool.repl_inst_filehdr->lms_group_info))
	{	/* This is the first time this instance is being brought up either as a root primary or as a propagating
		 * primary. Initialize the "lms_group_info" fields in the instance file header in journal pool shared memory.
		 * They will be flushed to the instance file as part of the "repl_inst_histinfo_add -> repl_inst_flush_filehdr"
		 * function invocation below.
		 */
		assert('\0' == jnlpool.repl_inst_filehdr->lms_group_info.created_nodename[0]);
		assert('\0' == jnlpool.repl_inst_filehdr->lms_group_info.this_instname[0]);
		assert(!jnlpool.repl_inst_filehdr->lms_group_info.creator_pid);
		jnlpool.repl_inst_filehdr->lms_group_info = jnlpool.repl_inst_filehdr->inst_info;
		assert('\0' != jnlpool.repl_inst_filehdr->lms_group_info.created_nodename[0]);
		DBG_CHECK_CREATED_NODENAME(jnlpool.repl_inst_filehdr->lms_group_info.created_nodename);
		assert('\0' != jnlpool.repl_inst_filehdr->lms_group_info.this_instname[0]);
		assert(jnlpool.repl_inst_filehdr->lms_group_info.created_time);
		assert(jnlpool.repl_inst_filehdr->lms_group_info.creator_pid);
	}
	/* Initialize histinfo fields */
	memcpy(histinfo.root_primary_instname, jnlpool.repl_inst_filehdr->inst_info.this_instname, MAX_INSTNAME_LEN - 1);
	histinfo.root_primary_instname[MAX_INSTNAME_LEN - 1] = '\0';
	assert('\0' != histinfo.root_primary_instname[0]);
	histinfo.start_seqno = start_seqno;
	assert(jnlpool.jnlpool_ctl->strm_seqno[0] == jnlpool.repl_inst_filehdr->strm_seqno[0]);
	assert(jnlpool.repl_inst_filehdr->is_supplementary || (0 == jnlpool.jnlpool_ctl->strm_seqno[0]));
	histinfo.strm_seqno = (!jnlpool.repl_inst_filehdr->is_supplementary) ? 0 : jnlpool.jnlpool_ctl->strm_seqno[0];
	histinfo.root_primary_cycle = jnlpool.repl_inst_filehdr->root_primary_cycle;
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
		rel_lock(jnlpool.jnlpool_dummy_reg);
}
