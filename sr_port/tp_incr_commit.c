/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "copy.h"
#include "tp_incr_commit.h"

GBLREF	uint4			dollar_tlevel;
GBLREF  sgm_info        	*first_sgm_info;
GBLREF 	sgmnt_data_ptr_t	cs_data;
GBLREF	global_tlvl_info	*global_tlvl_info_head;
GBLREF	buddy_list		*global_tlvl_info_list;

/* commit for transaction level greater than one */

void tp_incr_commit(void)
{
	sgm_info 		*si;
	cw_set_element 		*cse, *orig_cse, *prev_cse, *next_cse, *low_cse, *lower_cse;
	tlevel_info		*tli, *prev_tli = NULL, *last_prev_tli = NULL;
	global_tlvl_info 	*gtli, *prev_gtli;
	srch_blk_status		*tp_srch_status;
	ht_ent_int4		*tabent;

	for (si = first_sgm_info;  si != NULL;  si = si->next_sgm_info)
	{
		for (cse = si->first_cw_set; cse; cse = orig_cse->next_cw_set)
		{
			orig_cse = cse;
			TRAVERSE_TO_LATEST_CSE(cse);
			assert(dollar_tlevel >= cse->t_level);
			if (dollar_tlevel == cse->t_level)
			{
				cse->t_level--;
				low_cse = cse->low_tlevel;
				if (low_cse && low_cse->t_level == cse->t_level)	/* delete the duplicate link */
				{
					lower_cse = low_cse->low_tlevel;
					assert((low_cse->done && low_cse->new_buff) || (n_gds_t_op < cse->mode));
					if (lower_cse)
					{
						assert(lower_cse->t_level < cse->t_level);
						lower_cse->high_tlevel = cse;
						cse->low_tlevel = lower_cse;
						if (!cse->new_buff)
						{	/* if we never needed to build in the new level, copy the built copy
							 * (if any) of the older level before going back to that level
							 */
							assert(!cse->done);
							cse->new_buff = low_cse->new_buff;
						} else if (low_cse->new_buff)
							free_element(si->new_buff_list, (char *)low_cse->new_buff);
						free_element(si->tlvl_cw_set_list, (char *)low_cse);
						orig_cse = cse;
					} else
					{	/* In this case, there are only two elements in the horizontal list out of
						 * which we are going to delete one. We prefer to copy the second link into
						 * the first and delete the second (rather than simply deleting the first), since
						 * the first element may be an intermediate element in the vertical list and
						 * buddy list wont permit use of both free_element() and free_last_n_elements()
						 * with a given list together. This might disturb the tp_srch_status->cse, so
						 * reset it properly. Note that if cse->mode is gds_t_create, there will be no
						 * tp_srch_status entry allotted for cse->blk (one will be there only for the
						 * chain.flag representation of this to-be-created block). Same case with mode of
						 * kill_t_create as it also corresponds to a non-existent block#. Therefore dont
						 * try looking up the hashtable for this block in those cases.
						 */
						assert((gds_t_create == cse->mode) || (kill_t_create == cse->mode)
							|| (gds_t_write == cse->mode) || (kill_t_write == cse->mode));
						if ((gds_t_create != cse->mode) && (kill_t_create != cse->mode))
						{
							if (NULL != (tabent = lookup_hashtab_int4(si->blks_in_use,
													(uint4 *)&cse->blk)))
								tp_srch_status = tabent->value;
							else
								tp_srch_status = NULL;
							assert(!tp_srch_status || tp_srch_status->cse == cse);
							if (tp_srch_status)
								tp_srch_status->cse = low_cse;
						}
						assert(low_cse == orig_cse);
						/* Members that may not be uptodate in cse need to be copied back from low_cse.
						 * They are next_cw_set, prev_cw_set, new_buff and done.
						 */
						prev_cse = low_cse->prev_cw_set;
						next_cse = low_cse->next_cw_set;
						if (!cse->new_buff)
						{	/* if we never needed to build in the new level, copy the
							 * built copy of the older level before going back to that level
							 */
							assert(!cse->done);
							cse->new_buff = low_cse->new_buff;
						} else if (low_cse->new_buff)
							free_element(si->new_buff_list, (char *)low_cse->new_buff);
						memcpy(low_cse, cse, SIZEOF(cw_set_element));
						low_cse->next_cw_set = next_cse;
						low_cse->prev_cw_set = prev_cse;
						low_cse->high_tlevel = NULL;
						low_cse->low_tlevel = NULL;
						free_element(si->tlvl_cw_set_list, (char *)cse);
						orig_cse = low_cse;
					}
				} else
					assert(low_cse || orig_cse == cse);
			}
		}/* for (cse) */

		/* delete the tlvl_info for this t_level */
		for (tli = si->tlvl_info_head; tli; tli = tli->next_tlevel_info)
		{
			if (tli->t_level >= dollar_tlevel)
				break;
			prev_tli = tli;
		}
		assert(!tli || !tli->next_tlevel_info);
		if (prev_tli)
			prev_tli->next_tlevel_info = NULL;
		else
			si->tlvl_info_head = NULL;
		if (tli)
			free_last_n_elements(si->tlvl_info_list, 1);

	}/* for (si) */
	/* delete the global (across all segments) tlvl info for this t_level */
	for (prev_gtli = NULL, gtli = global_tlvl_info_head; gtli; gtli = gtli->next_global_tlvl_info)
	{
		if (dollar_tlevel <= gtli->t_level)
			break;
		prev_gtli = gtli;
	}
	assert(!global_tlvl_info_head || gtli);
	assert(!gtli || !gtli->next_global_tlvl_info);
	assert(!prev_gtli || (gtli && (dollar_tlevel == gtli->t_level)));
	FREE_GBL_TLVL_INFO(gtli);
	if (prev_gtli)
		prev_gtli->next_global_tlvl_info = NULL;
	else
		global_tlvl_info_head = NULL;
}
