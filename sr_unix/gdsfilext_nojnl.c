/****************************************************************
 *								*
 * Copyright (c) 2012-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_string.h"

#include "gtm_multi_thread.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblkops.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "min_max.h"
#include "t_qread.h"
#include "dse.h"
#include "gtmmsg.h"
#include "t_begin.h"
#include "t_write_map.h"
#include "t_abort.h"
#include "t_retry.h"
#include "t_end.h"
#include "wbox_test_init.h"
#include "error.h"
#include "t_recycled2free.h"
#include "cdb_sc.h"
#include "eintr_wrappers.h"
#include "gtmimagename.h"
#include "gdsfilext_nojnl.h"
#include "gtmio.h"
#include "anticipatory_freeze.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "copy.h"
#include "shmpool.h"
#include "db_write_eof_block.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

GBLREF	jnl_gbls_t	jgbl;

error_def(ERR_DBFILERR);

/* Minimal file extend. Called (at the moment) from mur_back_process.c when processing JRT_TRUNC record.
 * We want to avoid jnl and other interferences of gdsfilext.
 */
/* #GTM_THREAD_SAFE : The below function (gdsfilext_nojnl) is thread-safe */
int gdsfilext_nojnl(gd_region* reg, uint4 new_total, uint4 old_total)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	int 			blk_size, status;
	off_t			offset;
	char			*newmap, *aligned_buff;
	uint4			ii;
	unix_db_info		*udi;
	reg_ctl_list		*rctl;
	dio_buff_t		*diobuff;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(old_total < new_total);
	assert(new_total <= MAXTOTALBLKS(csd));
	blk_size = csd->blk_size;
	offset = (off_t)BLK_ZERO_OFF(csd->start_vbn) + (off_t)new_total * blk_size;
	if (udi->fd_opened_with_o_direct)
	{
		if (multi_thread_in_use)
		{	/* If multiple threads are running, we cannot use the global variable "dio_buff". Fortunately though,
			 * the only caller of this function which can have "multi_thread_in_use" set is "mur_back_process"
			 * which is invoked by a MUPIP JOURNAL command. Assert that. Given that, we can safely get at "rctl"
			 * from csa->miscptr in this case and use "rctl->dio_buff" safely inside threaded code since each thread
			 * operates on one "rctl".
			 */
			assert(jgbl.in_mupjnl);
			rctl = (reg_ctl_list *)csa->miscptr;
			assert(csa == rctl->csa);
			assert(csd == rctl->csd);
			DIO_BUFF_EXPAND_IF_NEEDED(udi, blk_size, &rctl->dio_buff);
			diobuff = &rctl->dio_buff;
		} else
			diobuff = &(TREF(dio_buff));
	}
	status = db_write_eof_block(udi, udi->fd, blk_size, offset, diobuff);
	if (0 != status)
	{
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		return status;
	}
	/* initialize the bitmap tn's to 0. */
	newmap = (char *)malloc(blk_size);
	bml_newmap((blk_hdr *)newmap, BM_SIZE(BLKS_PER_LMAP), 0);
	if (udi->fd_opened_with_o_direct)
	{
		aligned_buff = diobuff->aligned;
		memcpy(aligned_buff, newmap, blk_size);
	} else
		aligned_buff = newmap;
	/* initialize bitmaps, if any new ones are added */
	for (ii = ROUND_UP(old_total, BLKS_PER_LMAP); ii < new_total; ii += BLKS_PER_LMAP)
	{
		offset = (off_t)BLK_ZERO_OFF(csd->start_vbn) + (off_t)ii * blk_size;
		DB_LSEEKWRITE(csa, udi, udi->fn, udi->fd, offset, aligned_buff, blk_size, status);
		if (0 != status)
		{
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
			free(newmap);
			return status;
		}
	}
	csa->ti->free_blocks += DELTA_FREE_BLOCKS(new_total, old_total);
	csa->ti->total_blks = new_total;
	free(newmap);
	return status;
}
