/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"

#include <math.h> /* needed for handling of epoch_interval (EPOCH_SECOND2SECOND macro uses ceil) */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

void set_jnl_info(gd_region *reg, jnl_create_info *jnl_info)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	uint4			align_autoswitch;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	/* since journaling is already enabled at this stage, the MUPIP SET JOURNAL/MUPIP CREATE command that enabled journaling
	 * 	should have initialized the journal options in the db file header to default values.
	 * therefore all except jnl_deq (extension) should be non-zero. the following asserts check for that.
	 */
	assert(csa->now_crit);
	assert(csd->jnl_alq);
	assert(csd->alignsize);
	assert(csd->autoswitchlimit);
	assert(csd->jnl_buffer_size);
	assert(csd->epoch_interval);
	/* note that csd->jnl_deq can be 0 since a zero journal extension size is accepted */
	jnl_info->alignsize = csd->alignsize;
	jnl_info->before_images = csd->jnl_before_image;
	jnl_info->buffer = csd->jnl_buffer_size;
	jnl_info->epoch_interval = csd->epoch_interval;
	jnl_info->fn = (char *)reg->dyn.addr->fname;
	jnl_info->fn_len = reg->dyn.addr->fname_len;
	jnl_info->jnl = (char *)csd->jnl_file_name;
	jnl_info->jnl_len = csd->jnl_file_len;
	jnl_info->prev_jnl_len = csd->jnl_file_len;
	memcpy(jnl_info->prev_jnl, csd->jnl_file_name, jnl_info->prev_jnl_len);
	jnl_info->prev_jnl[jnl_info->prev_jnl_len] = '\0';
	QWASSIGN(jnl_info->reg_seqno, csd->reg_seqno);
	jnl_info->repl_state = csd->repl_state;
	jnl_info->rsize = csd->blk_size + DISK_BLOCK_SIZE;
	jnl_info->tn = csd->trans_hist.curr_tn;
	if (csa->jnl->alq_deq_auto_override)
	{	/* update csd to reflect the stored alq/deq/auto settings */
		assert(csd->autoswitchlimit < csa->jnl->autoswitchlimit); /* since otherwise alq_deq_auto_override would be FALSE */
		csd->autoswitchlimit = csa->jnl->autoswitchlimit;
		csd->jnl_alq = csa->jnl->jnl_alq;
		csd->jnl_deq = csa->jnl->jnl_deq;
		DEBUG_ONLY(csa->jnl->alq_deq_auto_override = ALQ_DEQ_AUTO_OVERRIDE_INVALID;)
		csa->jnl->alq_deq_auto_override = FALSE;
		/* although csd gets updated above, we do not flush it here. we instead rely on rundown to do the flush for us */
	}
	jnl_info->alloc = csd->jnl_alq;
	jnl_info->extend = csd->jnl_deq;
	jnl_info->autoswitchlimit = csd->autoswitchlimit;
	/* ensure autoswitchlimit is aligned to the nearest extension boundary
	 * since set_jnl_info only uses already established allocation/extension/autoswitchlimit values,
	 * 	as long as the establisher (MUPIP SET JOURNAL/MUPIP CREATE) ensures that autoswitchlimit is aligned
	 * 	we do not need to do anything here.
	 * t_end/tp_tend earlier used to round up their transaction's journal space requirements
	 * 	to the nearest extension boundary to compare against the autoswitchlimit later.
	 * but now with autoswitchlimit being aligned at an extension boundary, they can
	 * 	compare their journal requirements directly against the autoswitchlimit.
	 */
	assert(jnl_info->autoswitchlimit == ALIGNED_ROUND_DOWN(jnl_info->autoswitchlimit, jnl_info->alloc, jnl_info->extend));
}
