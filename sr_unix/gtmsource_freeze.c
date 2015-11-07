/****************************************************************
 *								*
 *	Copyright 2012, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_sem.h"
#include "repl_shutdcode.h"
#include "filestruct.h"
#include "util.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

error_def(ERR_REPLINSTFREEZECOMMENT);
error_def(ERR_REPLINSTFROZEN);
error_def(ERR_REPLINSTUNFROZEN);

int gtmsource_showfreeze(void)
{
	boolean_t instance_frozen;

	assert(!holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	instance_frozen = jnlpool.jnlpool_ctl->freeze;
	util_out_print("Instance Freeze: !AZ", TRUE, instance_frozen ? "ON" : "OFF");
	if (jnlpool.jnlpool_ctl->freeze)
		util_out_print(" Freeze Comment: !AZ", TRUE, jnlpool.jnlpool_ctl->freeze_comment);
	return (instance_frozen ? (SRV_ERR + NORMAL_SHUTDOWN) : NORMAL_SHUTDOWN);
}

int gtmsource_setfreeze(void)
{
	if (gtmsource_options.freezeval)
	{
		assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK); /* sets gtmsource_state */
	} else
		assert(!holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	jnlpool.jnlpool_ctl->freeze = gtmsource_options.freezeval;
	if (gtmsource_options.setcomment)
		STRNCPY_STR(jnlpool.jnlpool_ctl->freeze_comment, gtmsource_options.freeze_comment,
			SIZEOF(jnlpool.jnlpool_ctl->freeze_comment));
	if (gtmsource_options.freezeval)
	{
		send_msg(VARLSTCNT(3) ERR_REPLINSTFROZEN, 1, jnlpool.repl_inst_filehdr->inst_info.this_instname);
		send_msg(VARLSTCNT(3) ERR_REPLINSTFREEZECOMMENT, 1, jnlpool.jnlpool_ctl->freeze_comment);
		rel_lock(jnlpool.jnlpool_dummy_reg);
	} else
	{
		send_msg(VARLSTCNT(3) ERR_REPLINSTUNFROZEN, 1, jnlpool.repl_inst_filehdr->inst_info.this_instname);
	}
	return (NORMAL_SHUTDOWN);
}
