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
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "gvcst_blk_build.h"
#include "gvcst_delete_blk.h"

GBLREF	kill_set	*kill_set_tail;
GBLREF	sgm_info	*sgm_info_ptr;
GBLREF	uint4		dollar_tlevel;
GBLREF  gv_namehead	*gv_target;
GBLREF  boolean_t	horiz_growth;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	unsigned int	t_tries;
#ifdef VMS
GBLREF	boolean_t	tp_has_kill_t_cse; /* cse->mode of kill_t_write or kill_t_create got created in this transaction */
#endif

void	gvcst_delete_blk(block_id blk, int level, boolean_t committed)
{
	cw_set_element	*cse, *old_cse;
	kill_set	*ks;
	off_chain	chain;
	srch_blk_status	*tp_srch_status;
	uint4		iter;
	ht_ent_int4	*tabent;
	DEBUG_ONLY(
	boolean_t	block_already_in_hist = FALSE;
	)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	horiz_growth = FALSE;
	if (!dollar_tlevel)
		ks = kill_set_tail;
	else
	{
		PUT_LONG(&chain, blk);
		tp_srch_status = NULL;
		if (chain.flag == 1)
			tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain.cw_index, &cse);
		else
		{
			if (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use, (uint4 *)&blk)))
				tp_srch_status = (srch_blk_status *)tabent->value;
			cse = tp_srch_status ? tp_srch_status->cse : NULL;
		}
		if (cse)
		{
			if (!committed)
			{
				assert(dollar_tlevel >= cse->t_level);
	     			if (NULL != cse->high_tlevel)
				{	/* this is possible only if this block is already part of either of the tree paths
					 * in gv_target->hist or alt_hist (see gvcst_kill.c) and got a newer cse created as
					 * part of a gvcst_kill_blk on this block in gvcst_kill.c a little before the call
					 * to gvcst_kill_blk of a parent block which in turn decided to delete this block
					 * as part of its records that are getting removed. this is a guaranteed restartable
					 * situation. since gvcst_delete_blk does not return any status, proceed with the
					 * newer cse and return. the restart will be later detected by tp_hist.
					 */
					 cse = cse->high_tlevel;
					 DEBUG_ONLY(TREF(donot_commit) |= DONOTCOMMIT_GVCST_DELETE_BLK_CSE_TLEVEL;)
					 DEBUG_ONLY(block_already_in_hist = TRUE;)
				}
				assert(!cse->high_tlevel);
				if (cse->t_level != dollar_tlevel)
				{	/* this part of the code is almost similar to that in t_write(),
					 * any changes in one should be reflected in the other */
					horiz_growth = TRUE;
					old_cse = cse;
					cse = (cw_set_element *)get_new_free_element(sgm_info_ptr->tlvl_cw_set_list);
					memcpy(cse, old_cse, SIZEOF(cw_set_element));
					cse->low_tlevel = old_cse;
					cse->high_tlevel = NULL;
					old_cse->high_tlevel = cse;
					cse->t_level = dollar_tlevel;
					assert(2 == (SIZEOF(cse->undo_offset) / SIZEOF(cse->undo_offset[0])));
					assert(2 == (SIZEOF(cse->undo_next_off) / SIZEOF(cse->undo_next_off[0])));
					for (iter = 0; iter < 2; iter++)
						cse->undo_next_off[iter] = cse->undo_offset[iter] = 0;
					if (old_cse->done)
					{
						assert(NULL != old_cse->new_buff);
						cse->new_buff = (unsigned char *)get_new_free_element(sgm_info_ptr->new_buff_list);
						memcpy(cse->new_buff, old_cse->new_buff, ((blk_hdr_ptr_t)old_cse->new_buff)->bsiz);
					} else
						cse->new_buff = NULL;
					assert(!block_already_in_hist);
					/* tp_hist (called from gvcst_kill) updates "->cse" fields for all blocks that are
					 * part of the left or right histories of the M-kill. But this block is not one of
					 * those. Hence tp_srch_status->cse has to be updated here explicitly.
					 */
					if (tp_srch_status)
						tp_srch_status->cse = (void *)cse;
				}
				switch (cse->mode)
				{
				case gds_t_create:
					cse->mode = kill_t_create;
					VMS_ONLY(tp_has_kill_t_cse = TRUE;)
					if (level == 0)
					    return;
					break;
				case gds_t_write:
					cse->mode = kill_t_write;
					VMS_ONLY(tp_has_kill_t_cse = TRUE;)
					break;
				default:
					;
				}
			} else
			{
				switch(cse->mode)
				{
				case kill_t_create:
					VMS_ONLY(assert(tp_has_kill_t_cse));
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
			ks = sgm_info_ptr->kill_set_tail = sgm_info_ptr->kill_set_head = (kill_set *)malloc(SIZEOF(kill_set));
			ks->used = 0;
			ks->next_kill_set = NULL;
		}
	}
	while (ks->used >= BLKS_IN_KILL_SET)
	{
		if (ks->next_kill_set == NULL)
		{
			ks->next_kill_set = (kill_set *)malloc(SIZEOF(kill_set));
			ks->next_kill_set->used = 0;
			ks->next_kill_set->next_kill_set = NULL;
		}
		ks = kill_set_tail
		   = ks->next_kill_set;
	}
	ks->blk[ks->used].level = (level) ? 1 : 0;
	if (!dollar_tlevel || !chain.flag)
	{
		assert((CDB_STAGNATE > t_tries) || (blk < cs_addrs->ti->total_blks));
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
