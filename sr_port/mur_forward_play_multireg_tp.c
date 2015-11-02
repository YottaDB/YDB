/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gdskill.h"		/* needed for tp.h */
#include "gdscc.h"		/* needed for tp.h */
#include "tp.h"
#include "iosp.h"
#include "tp_change_reg.h"
#include "op_tcommit.h"

GBLREF 	mur_gbls_t		murgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	uint4			dollar_tlevel;
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	jnl_fence_control	jnl_fence_ctl;

error_def(ERR_JNLREADEOF);

/* Now that we are ready to apply the multi-region TP transaction, check the most recent "forw_multi->recstat" status
 * and propagate that to all the participating "rctl"s in the first_tp_rctl list. Use the following macro to implement that.
 */
#define	SET_RCTL_PROCESS_LOSTTN_IF_NEEDED(FORW_MULTI, RCTL)	\
{								\
	assert(!rctl->forw_eof_seen);				\
	if (GOOD_TN != FORW_MULTI->recstat)			\
		RCTL->process_losttn = TRUE;			\
}

uint4	mur_forward_play_multireg_tp(forw_multi_struct *forw_multi, reg_ctl_list *rctl)
{
	enum jnl_record_type	rectype;
	enum broken_type	recstat;
	jnl_tm_t		rec_time;
	uint4			status;
	seq_num 		rec_token_seq;
	jnl_record		*rec;
	jnl_ctl_list		*jctl;
	reg_ctl_list		*save_rctl, *next_rctl;
	uint4			num_tcoms, num_participants;
	boolean_t		tcom_played, first_tcom, deleted;
	ht_ent_int8		*tabent;
	forw_multi_struct	*cur_forw_multi, *prev_forw_multi;

	save_rctl = rctl;	/* save input "rctl" (needed at end) */
	assert(!save_rctl->forw_eof_seen);
	assert(1 < forw_multi->num_reg_total);	/* should not have come here for single-region TP transactions */
	rec_token_seq = forw_multi->token;
	recstat = forw_multi->recstat;
	if (mur_options.rollback && (rec_token_seq >= murgbl.losttn_seqno) && (GOOD_TN == recstat))
		recstat = forw_multi->recstat = LOST_TN;
	next_rctl = forw_multi->first_tp_rctl;
	assert(NULL != next_rctl);
	num_tcoms = 0;
	first_tcom = TRUE;
	assert(!dollar_tlevel);
	DEBUG_ONLY(num_participants = (uint4)-1;)	/* a very high value to indicate uninitialized state */
	do
	{
		rctl = next_rctl;
		next_rctl = rctl->next_tp_rctl;
		assert(NULL != next_rctl);
		SET_RCTL_PROCESS_LOSTTN_IF_NEEDED(forw_multi, rctl);
		assert(!rctl->forw_eof_seen);
		assert(num_tcoms < num_participants);
		MUR_CHANGE_REG(rctl);
		jctl = rctl->jctl;
		tcom_played = FALSE;
		for (status = SS_NORMAL; SS_NORMAL == status; status = mur_next_rec(&jctl))
		{
			if (tcom_played)
				break;
			rec = rctl->mur_desc->jnlrec;
			rectype = (enum jnl_record_type)rec->prefix.jrec_type;
			rec_time = rec->prefix.time;
			if ((BROKEN_TN == recstat) && (JRT_ALIGN != rectype))
			{	/* Check if current record is not a TP transaction (only exception is an ALIGN record which
				 * could show up in the MIDDLE of a TP transaction) or has a <token,time> that is different.
				 * If so we have come to beginning of NEXT transaction. Break in this case (this function
				 * should play only records of the current multi-region TP transaction. In case of GOOD_TN
				 * recstat, we are guaranteed to see a TCOM record before seeing the NEXT transaction.
				 */
				if (!IS_TP(rectype) || (rec_time != forw_multi->time) || (rec_token_seq != GET_JNL_SEQNO(rec)))
					break;
			}
			assert(IS_TP(rectype) || (JRT_ALIGN == rectype));
#			ifdef DEBUG
			if (IS_TP(rectype))
			{
				assert(REC_HAS_TOKEN_SEQ(rectype));
				assert(rec_token_seq == GET_JNL_SEQNO(rec));
			}
#			endif
			status = mur_forward_play_cur_jrec(rctl);
			if (SS_NORMAL != status)
				return status;
			assert(!murgbl.ok_to_update_db || dollar_tlevel || (GOOD_TN != recstat));
			assert(!murgbl.ok_to_update_db || !dollar_tlevel || (GOOD_TN == recstat));
			if (IS_COM(rectype))
			{
				if (first_tcom)
				{
					num_participants = rec->jrec_tcom.num_participants;
					first_tcom = FALSE;
				}
				assert(rec->jrec_tcom.num_participants == num_participants);
				num_tcoms++;
				if ((num_tcoms == num_participants) && murgbl.ok_to_update_db && (GOOD_TN == recstat))
				{	/* TCOM record of LAST region. Do actual transaction commit */
					MUR_SET_JNL_FENCE_CTL_TOKEN(rec_token_seq, ((reg_ctl_list *)NULL));
					jgbl.mur_jrec_participants = rec->jrec_tcom.num_participants;
					memcpy(tcom_record.jnl_tid, rec->jrec_tcom.jnl_tid, TID_STR_SIZE);
					assert(jnl_fence_ctl.token == rec->jrec_tcom.token_seq.token);
					assert(dollar_tlevel);
					op_tcommit();
					assert(!dollar_tlevel);
				}
				tcom_played = TRUE;
				MUR_FORW_TOKEN_REMOVE(rctl);
			}
		}
		CHECK_IF_EOF_REACHED(rctl, status); /* sets rctl->forw_eof_seen if needed; resets "status" to SS_NORMAL */
		if (SS_NORMAL != status)
			return status;
		if (rctl->forw_eof_seen)
			DELETE_RCTL_FROM_UNPROCESSED_LIST(rctl);
		if (NULL != rctl->forw_multi)
		{	/* Possible if we did not see TCOM. But has to be a BROKEN tn in that case.
			 * Treat this region as having completed token processing.
			 */
			assert(BROKEN_TN == recstat);
			MUR_FORW_TOKEN_REMOVE(rctl);
		}
	} while (next_rctl != rctl);
	assert((num_tcoms == num_participants) || (BROKEN_TN == recstat));
	assert(!dollar_tlevel);
	/* Now that the multi-region "forw_multi" structure is processed, it can be freed. Along with it, the corresponding
	 * hashtable entry can be freed as well as long as there are no other same-token "forw_multi" structures.
	 */
	tabent = forw_multi->u.tabent;
	assert(NULL != tabent);
	/* "tabent" should have been set before coming into this function. But make sure it is still correct
	 * (i.e. no other hash table expansions occurred in between).
	 */
	assert(tabent == lookup_hashtab_int8(&murgbl.forw_token_table, (gtm_uint64_t *)&rec_token_seq));
	if ((tabent->value == forw_multi) && (NULL == forw_multi->next))
	{	/* forw_multi is the ONLY element in the linked list so it is safe to delete the hashtable entry itself */
		deleted = delete_hashtab_int8(&murgbl.forw_token_table, &forw_multi->token);
		assert(deleted);
	} else
	{	/* delete "forw_multi" from the singly linked list */
		prev_forw_multi = NULL;
		cur_forw_multi = tabent->value;
		for ( ; (NULL != cur_forw_multi); prev_forw_multi = cur_forw_multi, cur_forw_multi = cur_forw_multi->next)
		{
			if (cur_forw_multi == forw_multi)
			{
				assert(prev_forw_multi != forw_multi);
				if (NULL == prev_forw_multi)
					tabent->value = cur_forw_multi->next;
				else
					prev_forw_multi->next = cur_forw_multi->next;
				break;
			}
		}
		assert(NULL != cur_forw_multi);
	}
	free_element(murgbl.forw_multi_list, (char *)forw_multi);
	MUR_CHANGE_REG(save_rctl); /* switch to region corresponding to input "rctl" before returning as caller relies on this */
	assert(!dollar_tlevel);
	assert(!save_rctl->forw_eof_seen || save_rctl->deleted_from_unprocessed_list);
	return (!save_rctl->forw_eof_seen ? SS_NORMAL : ERR_JNLREADEOF);
}
