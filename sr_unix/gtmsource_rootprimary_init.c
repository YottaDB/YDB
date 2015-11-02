/****************************************************************
 *								*
 *	Copyright 2006, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "repl_instance.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"

GBLREF	jnlpool_addrs		jnlpool;

/* This function is called primarily to append a new triple to the replication instance file by one of the following
 *	1) MUPIP REPLIC -SOURCE -START -ROOTPRIMARY command (after forking the child source server) if it created the journal pool.
 *	2) MUPIP REPLIC -SOURCE -ACTIVATE -ROOTPRIMARY command if this is a propagating primary to root primary transition.
 */
void	gtmsource_rootprimary_init(seq_num start_seqno)
{
	unix_db_info	*udi;
	repl_triple	triple;

	udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(udi->grabbed_ftok_sem);
	assert(NULL != jnlpool.repl_inst_filehdr);
	/* Update journal pool fields to reflect this is a root primary startup and updates are enabled */
	assert(!udi->s_addrs.hold_onto_crit);	/* this ensures we can safely do unconditional grab_lock and rel_lock */
	grab_lock(jnlpool.jnlpool_dummy_reg);
	jnlpool.repl_inst_filehdr->root_primary_cycle++;
	jnlpool.repl_inst_filehdr->was_rootprimary = TRUE;
	assert(start_seqno >= jnlpool.jnlpool_ctl->start_jnl_seqno);
	assert(start_seqno == jnlpool.jnlpool_ctl->jnl_seqno);
	jnlpool.repl_inst_filehdr->jnl_seqno = start_seqno;
	assert(jnlpool.jnlpool_ctl->upd_disabled);
	jnlpool.jnlpool_ctl->upd_disabled = FALSE;
	rel_lock(jnlpool.jnlpool_dummy_reg);
	/* Initialize triple fields */
	memset(&triple, 0, SIZEOF(repl_triple));
	memcpy(triple.root_primary_instname, jnlpool.repl_inst_filehdr->this_instname, MAX_INSTNAME_LEN - 1);
	assert('\0' != triple.root_primary_instname[0]);
	triple.start_seqno = start_seqno;
	triple.root_primary_cycle = jnlpool.repl_inst_filehdr->root_primary_cycle;
	triple.rcvd_from_instname[0] = '\0'; /* redundant due to the memset above but done for completeness of the initialization */
	/* Add the triple to the instance file and flush the changes in the journal pool to the file header */
	repl_inst_triple_add(&triple);
}
