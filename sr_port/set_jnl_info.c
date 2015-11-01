/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

void set_jnl_info(gd_region *reg, jnl_create_info *set_jnl_info)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;

	assert(csa->now_crit);
	set_jnl_info->alloc = (0 == csd->jnl_alq) ? JNL_ALLOC_DEF : csd->jnl_alq;
	set_jnl_info->extend = (0 == csd->jnl_deq) ? set_jnl_info->alloc * JNL_EXTEND_DEF_PERC : csd->jnl_deq;
	set_jnl_info->buffer = (0 == csd->jnl_buffer_size) ? JNL_BUFFER_DEF : csd->jnl_buffer_size;
	set_jnl_info->alignsize = (0 == csa->jnl->jnl_buff->alignsize) ? JNL_MIN_ALIGNSIZE : csa->jnl->jnl_buff->alignsize;
	set_jnl_info->epoch_interval = (0 == csa->jnl->jnl_buff->epoch_interval) ? DEFAULT_EPOCH_INTERVAL
											: csa->jnl->jnl_buff->epoch_interval;
	set_jnl_info->rsize = csd->blk_size + DISK_BLOCK_SIZE;
	set_jnl_info->fn = (char *)reg->dyn.addr->fname;
	set_jnl_info->fn_len = reg->dyn.addr->fname_len;
	set_jnl_info->jnl = (char *)csd->jnl_file_name;
	set_jnl_info->jnl_len = csd->jnl_file_len;
	set_jnl_info->prev_jnl_len = csd->jnl_file_len;
	memcpy(set_jnl_info->prev_jnl, csd->jnl_file_name, set_jnl_info->prev_jnl_len);
	set_jnl_info->prev_jnl[set_jnl_info->prev_jnl_len] = '\0';
	set_jnl_info->tn = csd->trans_hist.curr_tn;
	set_jnl_info->before_images = csd->jnl_before_image;
	set_jnl_info->repl_state = csd->repl_state;
	QWASSIGN(set_jnl_info->reg_seqno, csd->reg_seqno);
}
