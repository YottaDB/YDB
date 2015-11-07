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
#include "gdsblk.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include <rtnhdr.h>
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs structure definition */
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "repl_instance.h"
#include "gtmrecv.h"
#include "gtmimagename.h"
#include "have_crit.h"
#include "tp_frame.h"

GBLREF	gtmsource_state_t	gtmsource_state;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	seq_num			gtmsource_save_read_jnl_seqno;
GBLREF	uint4			process_id;

void	gtmsource_onln_rlbk_clnup()
{
	gtmsource_local_ptr_t	gtmsource_local;
	boolean_t		was_crit;
	sgmnt_addrs		*repl_csa;

	gtmsource_local = jnlpool.gtmsource_local;
	repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
	was_crit = repl_csa->now_crit;
	assert(!repl_csa->hold_onto_crit);
	assert(was_crit || (process_id == gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid)
		|| (0 != have_crit(CRIT_HAVE_ANY_REG)));
	/* Reset source server context to indicate a fresh connection that is about to take place */
	assert(NULL != gtmsource_local);
	if (NULL != gtmsource_local)
	{
		/* If ROLLBACK has not taken the instance past the source server's read_jnl_seqno, then the source server should
		 * just continue from where it currently is and start sending the journal records from that point onwards. But, this
		 * is non-trivial. The reason is because, when the source server detected the online rollback, it could be in the
		 * READ_POOL state. But, since the instance has been rolled back, the journal pool cannot be relied upon in its
		 * entirety. To illustrate this -- consider that the journal pool contains the data from 1-100 and the source server
		 * is currently sending sequence number 30 and is reading from the pool. Assume an online rollback happens that
		 * takes the instance from sequence number 100 to sequence number 80 and leaves the journal pool write_addr and
		 * early_write_addr untouched. Now, lets say GT.M process comes in after this and does a few more updates. All of
		 * these updates will be written in the journal pool right after the "old-rolled-back" sequence number 100. If the
		 * source server continues to read from the pool, it will send the valid data until sequence number 80. After that,
		 * it will start sending the "old-rolled-back" sequence numbers 81-100 which is not right. To avoid this, rollback
		 * should set the write_addr and early_write_addr by searching in the journal pool for sequence number 81. This is
		 * currently not done, but is something that we can think about when it comes to optimization. Until then, force
		 * rollback to reset jnlpool's write_addr, write and early_write_addr to 0 and let source server be forced into
		 * READ_FILE mode.
		 */
		gtmsource_local->read_state = READ_FILE;
		/* Set the state which gets bubbled up the call chain to gtmsource_process at which point we will close and
		 * re-establish the connection with the other end.
		 */
		gtmsource_local->gtmsource_state = gtmsource_state = GTMSOURCE_HANDLE_ONLN_RLBK;
		if (!was_crit)
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		/* We have to let the read files logic know that until we have sent data "upto" the current journal sequence number
		 * at this point, we cannot rely on the journal pool. Indicate this through the gtmsource_save_read_jnl_seqno global
		 * variable
		 */
		gtmsource_save_read_jnl_seqno = jnlpool.jnlpool_ctl->jnl_seqno;
		gtmsource_local->read = jnlpool.jnlpool_ctl->write;
		gtmsource_local->read_addr = jnlpool.jnlpool_ctl->write_addr;
		if (!was_crit)
			rel_lock(jnlpool.jnlpool_dummy_reg);
	}
	return;
}
