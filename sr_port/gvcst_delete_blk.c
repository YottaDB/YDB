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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "gvcst_blk_build.h"
#include "gvcst_delete_blk.h"

GBLREF	kill_set	*kill_set_tail;
GBLREF	sgm_info	*sgm_info_ptr;
GBLREF	short		dollar_tlevel;
GBLREF  gv_namehead	*gv_target;
GBLREF  boolean_t	horiz_growth;

void	gvcst_delete_blk(block_id blk, int level, boolean_t committed)
{
	cw_set_element	*cse, *old_cse;
	kill_set	*ks;
	off_chain	chain;
	srch_blk_status	*tp_srch_status;
	uint4		dummy, iter;

	/* an assert to verify the validity of the block number was removed
	 * because it could be triggered by a concurrency conflict
	 */

	horiz_growth = FALSE;
	if (dollar_tlevel == 0)
		ks = kill_set_tail;
	else
	{
		PUT_LONG(&chain, blk);
		tp_srch_status = NULL;
		if (chain.flag == 1)
			tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain.cw_index, &cse);
		else
		{
			tp_srch_status = (srch_blk_status *)lookup_hashtab_ent(sgm_info_ptr->blks_in_use,
											(void *)blk, &dummy);
			cse = tp_srch_status ? tp_srch_status->ptr : NULL;
		}
		assert(!cse || !cse->high_tlevel);
		if (cse)
		{
			if (!committed)
			{
				assert(dollar_tlevel >= cse->t_level);
				if (cse->t_level != dollar_tlevel)
				{
					/* this part of the code is almost similar to that in t_write(),
					 * any changes in one should be reflected in the other */
					horiz_growth = TRUE;
					old_cse = cse;
					cse = (cw_set_element *)get_new_free_element(sgm_info_ptr->tlvl_cw_set_list);
					memcpy(cse, old_cse, sizeof(cw_set_element));
					cse->low_tlevel = old_cse;
					cse->high_tlevel = NULL;
					old_cse->high_tlevel = cse;
					cse->t_level = dollar_tlevel;
					assert(2 == (sizeof(cse->undo_offset) / sizeof(cse->undo_offset[0])));
					assert(2 == (sizeof(cse->undo_next_off) / sizeof(cse->undo_next_off[0])));
					for (iter = 0; iter < 2; iter++)
						cse->undo_next_off[iter] = cse->undo_offset[iter] = 0;
					if (!old_cse->new_buff)		/* it's possible to arrive here with an unbuilt block */
						gvcst_blk_build(old_cse, (uchar_ptr_t)old_cse->new_buff, 0);
					old_cse->done = TRUE;
					cse->new_buff = ((new_buff_buddy_list *)
								get_new_free_element(sgm_info_ptr->new_buff_list))->new_buff;
					memcpy(cse->new_buff, old_cse->new_buff, ((blk_hdr_ptr_t)old_cse->new_buff)->bsiz);
					/* tp_srch_status->ptr has to be updated here, since gvcst_kill() does
					 * not call tp_hist() at the end as in gvcst_put_blk() */
					if (tp_srch_status)
						tp_srch_status->ptr = (void *)cse;
				}
				switch (cse->mode)
				{
				case gds_t_create:
					cse->mode = kill_t_create;
					if (level == 0)
					    return;
					break;
				case gds_t_write:
					cse->mode = kill_t_write;
					break;
				default:
					;
				}
			} else
			{
				switch(cse->mode)
				{
				case kill_t_create:
					if (level == 0)
						return;
					break;
				default:
					if (chain.flag)
					{
						chain.flag = 0;
						blk = cse->blk;
					}
					break;
				}
			}
		}
		ks = sgm_info_ptr->kill_set_tail;
		if (NULL == ks)		/* Allocate first kill set to sgm_info_ptr block */
		{
			ks = sgm_info_ptr->kill_set_tail = sgm_info_ptr->kill_set_head = (kill_set *)malloc(sizeof(kill_set));
			ks->used = 0;
			ks->next_kill_set = NULL;
		}
	}
	while (ks->used >= BLKS_IN_KILL_SET)
	{
		if (ks->next_kill_set == NULL)
		{
			ks->next_kill_set = (kill_set *)malloc(sizeof(kill_set));
			ks->next_kill_set->used = 0;
			ks->next_kill_set->next_kill_set = NULL;
		}
		ks = kill_set_tail
		   = ks->next_kill_set;
	}
	ks->blk[ks->used].level = level;
	if (dollar_tlevel == 0 || chain.flag == 0)
	{
		ks->blk[ks->used].block = blk;
		ks->blk[ks->used].flag = 0;
	} else
	{
	    	ks->blk[ks->used].block = chain.cw_index;
	    	ks->blk[ks->used].flag = chain.flag;
	}
	++ks->used;
	assert(ks->used <= BLKS_IN_KILL_SET);
}
