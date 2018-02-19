/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"		/* needed for jnlpool_addrs_ptr_t */
#include "gtmsource.h"		/* needed for jnlpool_addrs_ptr_t */
#include "secshr_db_clnup.h"
#include "sec_shr_blk_build.h"

GBLREF	cw_set_element		cw_set[];
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	unsigned char		cw_set_depth;

/* To check if an input CS->mode is gds_t_write_root, it is possible we are in phase2 in which case we would have
 * set CS->mode to gds_t_committed in which case CS->old_mode would hold the gds_t_write_root value hence the below macro.
 */
#define	IS_T_WRITE_ROOT(CS) ((gds_t_write_root == CS->mode)							\
					|| (gds_t_committed == CS->mode) && (gds_t_write_root == CS->old_mode))

/* Returns 0 if success, -1 if failure */
int secshr_blk_full_build(boolean_t is_tp, sgmnt_addrs *csa,
	 	sgmnt_data_ptr_t csd, boolean_t is_bg, struct cw_set_element_struct *cs, sm_uc_ptr_t blk_ptr, trans_num currtn)
{
	boolean_t	is_write_root;
	cw_set_element	*nxt, *cs_ptr;
	off_chain	chain;
	sgm_info	*si, *save_si;
	unsigned char	*chain_ptr;
	int		numargs;
	gtm_uint64_t	argarray[SECSHR_ACCOUNTING_MAX_ARGS];

	if (!is_tp)
	{	/* Non-TP */
		sec_shr_blk_build(csa, csd, cs, blk_ptr, currtn);
		assert(!IS_T_WRITE_ROOT(cs));
		is_write_root = FALSE;
		do
		{
			if (!cs->ins_off)
			{
				assert(!is_write_root);
				break;
			}
			if ((cs->ins_off > ((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
				|| (cs->ins_off < (SIZEOF(blk_hdr) + SIZEOF(rec_hdr)))
				|| (0 > (short)cs->index)
				|| ((cs - cw_set) <= cs->index))
			{
				numargs = 0;
				SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
				SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_blk_full_build);
				SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cs);
				SECSHR_ACCOUNTING(numargs, argarray, cs->blk);
				SECSHR_ACCOUNTING(numargs, argarray, cs->index);
				SECSHR_ACCOUNTING(numargs, argarray, cs->ins_off);
				SECSHR_ACCOUNTING(numargs, argarray, ((blk_hdr *)blk_ptr)->bsiz);
				secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
				assert(FALSE);
				return -1;
			}
			PUT_LONG((blk_ptr + cs->ins_off), ((cw_set_element *)(cw_set + cs->index))->blk);
			if (is_write_root)
				break;
			cs++;
			is_write_root = IS_T_WRITE_ROOT(cs);
			if ((cs >= (cw_set + cw_set_depth)) || !is_write_root)
				break;
		} while (TRUE);
	} else
	{	/* TP */
		si = csa->sgm_info_ptr;
		if (0 == cs->done)
		{
			sec_shr_blk_build(csa, csd, cs, blk_ptr, currtn);
			if (0 != cs->ins_off)
			{
				if ((cs->ins_off > ((blk_hdr *)blk_ptr)->bsiz - SIZEOF(block_id))
					|| (cs->ins_off < (SIZEOF(blk_hdr) + SIZEOF(rec_hdr))))
				{
					numargs = 0;
					SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
					SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_blk_full_build);
					SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cs);
					SECSHR_ACCOUNTING(numargs, argarray, cs->blk);
					SECSHR_ACCOUNTING(numargs, argarray, cs->index);
					SECSHR_ACCOUNTING(numargs, argarray, cs->ins_off);
					SECSHR_ACCOUNTING(numargs, argarray, ((blk_hdr *)blk_ptr)->bsiz);
					secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
					assert(FALSE);
					return -1;
				}
				if (0 == cs->first_off)
					cs->first_off = cs->ins_off;
				chain_ptr = blk_ptr + cs->ins_off;
				chain.flag = 1;
				/* Note: Currently only assert check of cs->index, not an if check like "cs->ins_off" above */
				assert((0 <= (short)cs->index));
				assert(cs->index < si->cw_set_depth);
				chain.cw_index = cs->index;
				chain.next_off = cs->next_off;
				GET_LONGP(chain_ptr, &chain);
				cs->ins_off = cs->next_off = 0;
			}
		} else
		{
			memmove(blk_ptr, cs->new_buff, ((blk_hdr *)cs->new_buff)->bsiz);
			((blk_hdr *)blk_ptr)->tn = currtn;
		}
		if (cs->first_off)
		{
			for (chain_ptr = blk_ptr + cs->first_off; ; chain_ptr += chain.next_off)
			{
				GET_LONGP(&chain, chain_ptr);
				if ((1 == chain.flag)
					&& ((chain_ptr - blk_ptr + SIZEOF(block_id)) <= ((blk_hdr *)blk_ptr)->bsiz)
					&& (chain.cw_index < si->cw_set_depth))
				{
					save_si = sgm_info_ptr;
					sgm_info_ptr = si;	/* needed by "tp_get_cw" */
					tp_get_cw(si->first_cw_set, chain.cw_index, &cs_ptr);
					sgm_info_ptr = save_si;
					PUT_LONG(chain_ptr, cs_ptr->blk);
					if (0 == chain.next_off)
						break;
				} else
				{
					numargs = 0;
					SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
					SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_blk_full_build);
					SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)cs);
					SECSHR_ACCOUNTING(numargs, argarray, cs->blk);
					SECSHR_ACCOUNTING(numargs, argarray, cs->index);
					SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)blk_ptr);
					SECSHR_ACCOUNTING(numargs, argarray, (INTPTR_T)chain_ptr);
					SECSHR_ACCOUNTING(numargs, argarray, chain.next_off);
					SECSHR_ACCOUNTING(numargs, argarray, chain.cw_index);
					SECSHR_ACCOUNTING(numargs, argarray, si->cw_set_depth);
					SECSHR_ACCOUNTING(numargs, argarray, ((blk_hdr *)blk_ptr)->bsiz);
					secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
					assert(FALSE);
					return -1;
				}
			}
		}
	}	/* TP */
	return 0;
}
