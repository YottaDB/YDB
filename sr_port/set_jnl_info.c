/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"


#include <stddef.h>
#include <math.h> /* needed for handling of epoch_interval (EPOCH_SECOND2SECOND macro uses ceil) */

#include "iosp.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "min_max.h"		/* for JNL_MAX_RECLEN macro */
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF 	jnl_gbls_t	jgbl;
DEBUG_ONLY(GBLREF	boolean_t	mupip_jnl_recover;)
void set_jnl_info(gd_region *reg, jnl_create_info *jnl_info)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	uint4			align_autoswitch, autoswitch_increase;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	/* since journaling is already enabled at this stage, the MUPIP SET JOURNAL/MUPIP CREATE command that enabled journaling
	 * 	should have initialized the journal options in the db file header to default values.
	 * therefore all except jnl_deq (extension) should be non-zero. the following asserts check for that.
	 */
	assert(mupip_jnl_recover || csa->now_crit);
	assert(csd->jnl_alq);
	assert(csd->alignsize);
	assert(csd->autoswitchlimit);
	assert(csd->jnl_buffer_size);
	assert(csd->epoch_interval);
	jnl_info->csd = csd;
	jnl_info->csa = csa;
	/* note that csd->jnl_deq can be 0 since a zero journal extension size is accepted */
	jnl_info->status = jnl_info->status2 = SS_NORMAL;
	jnl_info->no_rename = jnl_info->no_prev_link = FALSE;
	UNIX_ONLY(
		if ((JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE) > csd->alignsize)
		{	/* Possible due to the smaller JNL_MIN_ALIGNSIZE used in previous (pre-V60000) versions
			 * as opposed to the current alignsize of 2MB. Fix fileheader to be
			 * at least minimum (i.e. an on-the-fly upgrade of the db file header).
			 */
			csd->alignsize = (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE);
		}
	)
	jnl_info->alignsize = csd->alignsize;
	jnl_info->before_images = csd->jnl_before_image;
	jnl_info->buffer = csd->jnl_buffer_size;
	jnl_info->epoch_interval = csd->epoch_interval;
	jnl_info->fn = reg->dyn.addr->fname;
	jnl_info->fn_len = reg->dyn.addr->fname_len;
	jnl_info->jnl_len = csd->jnl_file_len;
	memcpy(jnl_info->jnl, csd->jnl_file_name, jnl_info->jnl_len);
	if (!jgbl.forw_phase_recovery)
		jnl_info->reg_seqno = csd->reg_seqno;
	else
		jnl_info->reg_seqno = jgbl.mur_jrec_seqno;
	jnl_info->jnl_state = csd->jnl_state;	/* Used in cre_jnl_file() */
	assert(JNL_ALLOWED(jnl_info));
	jnl_info->repl_state = csd->repl_state;
	JNL_MAX_RECLEN(jnl_info, csd);
	UNIX_ONLY(
		if (JNL_ALLOC_MIN > csd->jnl_alq)
		{	/* Possible if a pre-V54001 journaled db (which allows allocation values as low as 10) is used
			 * with V54001 and higher (where minimum allowed allocation value is 200). Fix fileheader to be
			 * at least minimum (an on-the-fly upgrade of the db file header).
			 */
			csd->jnl_alq = JNL_ALLOC_MIN;
		}
	)
	jnl_info->alloc = csd->jnl_alq;
	jnl_info->extend = csd->jnl_deq;
	/* ensure autoswitchlimit is aligned to the nearest extension boundary
	 * since set_jnl_info only uses already established allocation/extension/autoswitchlimit values,
	 * 	as long as the establisher (MUPIP SET JOURNAL/MUPIP CREATE) ensures that autoswitchlimit is aligned
	 * 	we do not need to do anything here.
	 * t_end/tp_tend earlier used to round up their transaction's journal space requirements
	 * 	to the nearest extension boundary to compare against the autoswitchlimit later.
	 * but now with autoswitchlimit being aligned at an extension boundary, they can
	 * 	compare their journal requirements directly against the autoswitchlimit.
	 */
	align_autoswitch = ALIGNED_ROUND_DOWN(csd->autoswitchlimit, csd->jnl_alq, csd->jnl_deq);
	if (align_autoswitch != csd->autoswitchlimit)
	{	/* round down specified autoswitch to be aligned at a journal extension boundary */
		assert(align_autoswitch < csd->autoswitchlimit || !csd->jnl_deq);
		autoswitch_increase = ((0 < csd->jnl_deq) ? csd->jnl_deq : DISK_BLOCK_SIZE);
		while (JNL_AUTOSWITCHLIMIT_MIN > align_autoswitch)
			align_autoswitch += autoswitch_increase;
		csd->autoswitchlimit = align_autoswitch;
	}
	jnl_info->autoswitchlimit = csd->autoswitchlimit;
	assert(jnl_info->autoswitchlimit == ALIGNED_ROUND_DOWN(jnl_info->autoswitchlimit, jnl_info->alloc, jnl_info->extend));
	jnl_info->blks_to_upgrd = csd->blks_to_upgrd; /* will be copied over to EPOCH record in newly created journal */
	jnl_info->free_blocks   = csd->trans_hist.free_blocks; /* will be copied over to EPOCH record in newly created journal */
	jnl_info->total_blks    = csd->trans_hist.total_blks; /* will be copied over to EPOCH record in newly created journal */
	GTMCRYPT_ONLY(
		GTMCRYPT_COPY_HASH(csd, jnl_info);
	)
}
