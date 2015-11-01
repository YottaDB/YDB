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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "copy.h"
#include "util.h"

#define ASSERT_MATCH(X,Y)	assert(&X.pini_addr == &Y.pini_addr); \
				assert(&X.short_time == &Y.short_time); \
				assert(&X.mumps_node == &Y.mumps_node); \
				assert(&X.tn == &Y.tn)


GBLDEF	int		participants;
GBLREF	mur_opt_struct	mur_options;
GBLREF  seq_num		consist_jnl_seqno;
GBLREF	seq_num		min_epoch_jnl_seqno;
GBLREF	seq_num		max_epoch_jnl_seqno;
GBLREF	seq_num		seq_num_zero, seq_num_minus_one;
GBLREF	char		*log_rollback;
GBLREF	broken_struct	*broken_array;
GBLREF	int4		mur_error_count;



/*
 *	This routine performs backward processing.  It returns TRUE unless either:
 *	     -	a disallowed error occurs;  OR
 *	     -	BACKWARD has been specified, and we've reached the "turn-around"
 *		point in the journal file.
 *
 *	The "turn-around" point is always the most recent EPOCH record whose
 *	timestamp is prior to the SINCE time, provided that there are no broken
 *	transactions that are still in progress at that point.  If there ARE,
 *	then it's the most recent EPOCH record at which there are no longer any
 *	outstanding broken transactions, or at which we have exceeded either of
 *	the LOOKBACK_LIMIT parameters, whichever occurs first.
 */

bool	mur_back_process(ctl_list *ctl)
{
	bool			proceed = TRUE, eof, broken;
	uint4			pini_addr;
	uint4			rec_time, status;
	token_num		token;
	jnl_record		*rec;
	jnl_process_vector	*pv;
	struct current_struct	*curr;
	enum jnl_record_type 	rectype;
	seq_num			local_jnl_seqno;
	seq_num			tempqw_seqno;
	uint4			tempdw_seqno;
	unsigned char		*ptr, qwstring[100];
	unsigned char		*ptr1, qwstring1[100];

	rec = (jnl_record *)ctl->rab->recbuff;

	rectype = REF_CHAR(&rec->jrec_type);

	switch(REF_CHAR(&rec->jrec_type))
	{
	default:
		proceed = mur_report_error(ctl, MUR_UNKNOWN);
		break;

	case JRT_ALIGN:
	case JRT_NULL:
		break;

	case JRT_INCTN:
	case JRT_AIMG:
		rec_time = rec->val.jrec_inctn.short_time;
		if (mur_options.lookback_opers_specified)
			++ctl->lookback_count;
		break;

	case JRT_PFIN:
		if (mur_lookup_current(ctl, rec->val.jrec_pfin.pini_addr) != NULL)
			/* Entries for this process were seen subsequent to the PFIN */
			GTMASSERT;
		/* drop through ... */

	case JRT_PINI:
		assert(&rec->val.jrec_pini.process_vector == &rec->val.jrec_pfin.process_vector);
		rec_time = JNL_S_TIME(rec, jrec_pini);
		break;

	case JRT_PBLK:
		rec_time = rec->val.jrec_pblk.short_time;
			/* Don't apply PBLK's to the database if VERIFY was specified,
		   		in case any errors are encountered before we reach the turn-around point */
		if (mur_options.update  &&  !mur_options.forward  &&  !mur_options.verify)
			mur_output_record(ctl);
		break;

	case JRT_EPOCH:
		rec_time = rec->val.jrec_epoch.short_time;
		if (mur_options.rollback)
		{
			ctl->turn_around_tn = rec->val.jrec_epoch.tn;
			QWASSIGN(local_jnl_seqno, rec->val.jrec_epoch.jnl_seqno);
			if (QWLE(local_jnl_seqno, consist_jnl_seqno))
			{
				proceed = FALSE;
				ctl->consist_stop_addr = ctl->rab->dskaddr;
				ptr = i2ascl(qwstring, local_jnl_seqno);
				ptr1 = i2asclx(qwstring1, local_jnl_seqno);
if (log_rollback)
	util_out_print("MUR-I-DEBUG : Journal !AD  -->  SECOND Stop Epoch Jnl Seqno = !AD [0x!AD]--> Consist_Stop_Addr = 0x!XL",
		TRUE, ctl->jnl_fn_len, ctl->jnl_fn, ptr - qwstring, qwstring, ptr1 - qwstring1, qwstring1, ctl->consist_stop_addr);
			}
			break;
		}

		ctl->turn_around_tn = rec->val.jrec_epoch.tn;
		if (rec_time > JNL_M_TIME(since_time)  ||  mur_options.forward)
			break;

		/* We've reached the SINCE time */

		/* Start counting LOOKBACK_LIMIT operations from here */
		ctl->reached_lookback_limit = rec_time < JNL_M_TIME(lookback_time)  ||
			(mur_options.lookback_opers_specified && ctl->lookback_count >= mur_options.lookback_opers);
		MID_TIME(ctl->lookback_time) = rec_time;
		/* proceed will be TRUE until we have reached the turn-around point */
		proceed = ctl->broken_entries > 0  &&  !ctl->reached_lookback_limit;
		if (FALSE == proceed)
			ctl->consist_stop_addr = ctl->rab->dskaddr;	/* Note down consist_stop_addr for recover too */
		break;

	case JRT_EOF:
		rec_time = JNL_S_TIME(rec, jrec_eof);
		eof = ctl->found_eof;
		ctl->found_eof = TRUE;
		proceed = !eof  ||  mur_report_error(ctl, MUR_MULTEOF);
		break;

	case JRT_SET:
	case JRT_KILL:
	case JRT_ZKILL:
		ASSERT_MATCH(rec->val.jrec_kill, rec->val.jrec_set);
		ASSERT_MATCH(rec->val.jrec_zkill, rec->val.jrec_set);
		rec_time = rec->val.jrec_set.short_time;

		if (mur_options.fences == FENCE_NONE)
			break;

		if (mur_options.fences == FENCE_PROCESS)
		{
			if (mur_lookup_current(ctl, rec->val.jrec_set.pini_addr) != NULL)
			{
				mur_delete_current(ctl, rec->val.jrec_set.pini_addr);
				proceed = mur_report_error(ctl, MUR_BRKTRANS);
				if (mur_options.rollback)
				{
					--mur_error_count;
					proceed = TRUE;
				}
			}
		}
		else
		{
			assert(mur_options.fences == FENCE_ALWAYS);

			proceed = mur_report_error(ctl, MUR_UNFENCE);
		}

		if (mur_options.lookback_opers_specified)
			++ctl->lookback_count;

		if (mur_options.rollback  &&  QWNE(seq_num_minus_one, min_epoch_jnl_seqno))
		{
			assert(&rec->val.jrec_kill.jnl_seqno ==  &rec->val.jrec_set.jnl_seqno);
			assert(&rec->val.jrec_zkill.jnl_seqno ==  &rec->val.jrec_set.jnl_seqno);
			assert(QWLT(rec->val.jrec_kill.jnl_seqno,  max_epoch_jnl_seqno));
			if (QWGE(rec->val.jrec_kill.jnl_seqno, consist_jnl_seqno))
			{
				QWSUB(tempqw_seqno, rec->val.jrec_kill.jnl_seqno, consist_jnl_seqno);
				DWASSIGNQW(tempdw_seqno, tempqw_seqno);
				broken_array[tempdw_seqno].count = -1;
			}
					/* littleton changes -- this doesn't take care of multiple files having the same jnlseqno */
		}
		break;

	case JRT_FSET:
	case JRT_TSET:
	case JRT_FKILL:
	case JRT_TKILL:
	case JRT_FZKILL:
	case JRT_TZKILL:
		ASSERT_MATCH(rec->val.jrec_fset, rec->val.jrec_tset);
		ASSERT_MATCH(rec->val.jrec_fset, rec->val.jrec_fkill);
		ASSERT_MATCH(rec->val.jrec_fset, rec->val.jrec_tkill);
		ASSERT_MATCH(rec->val.jrec_fset, rec->val.jrec_fzkill);
		ASSERT_MATCH(rec->val.jrec_fset, rec->val.jrec_tzkill);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_tset.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_fkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_tkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_fzkill.token);
		assert(&rec->val.jrec_fset.token ==  &rec->val.jrec_tzkill.token);
		assert(&rec->val.jrec_fset.jnl_seqno ==  &rec->val.jrec_tset.jnl_seqno);
		assert(&rec->val.jrec_fset.jnl_seqno ==  &rec->val.jrec_fkill.jnl_seqno);
		assert(&rec->val.jrec_fset.jnl_seqno ==  &rec->val.jrec_tkill.jnl_seqno);
		assert(&rec->val.jrec_fset.jnl_seqno ==  &rec->val.jrec_fzkill.jnl_seqno);
		assert(&rec->val.jrec_fset.jnl_seqno ==  &rec->val.jrec_tzkill.jnl_seqno);

		rec_time = rec->val.jrec_fset.short_time;

		if (mur_options.fences == FENCE_NONE  &&
			(REF_CHAR(&rec->jrec_type) == JRT_FSET	|| REF_CHAR(&rec->jrec_type) == JRT_FKILL
			 					|| REF_CHAR(&rec->jrec_type) == JRT_FZKILL))
			break;

		pini_addr = rec->val.jrec_fset.pini_addr;

		QWASSIGN(token, rec->val.jrec_fset.token);
		assert(QWEQ(token, rec->val.jrec_tset.token));
		assert(QWEQ(token, rec->val.jrec_fkill.token));
		assert(QWEQ(token, rec->val.jrec_tkill.token));
		assert(QWEQ(token, rec->val.jrec_fzkill.token));
		assert(QWEQ(token, rec->val.jrec_tzkill.token));

		if ((curr = mur_lookup_current(ctl, pini_addr)) == NULL)
		{
			if (!mur_options.selection  ||  mur_do_record(ctl))
			{
				mur_cre_broken(ctl, pini_addr, token);
				proceed = mur_report_error(ctl, MUR_BRKTRANS);
				if (mur_options.rollback)
				{
					--mur_error_count;
					proceed = TRUE;
				}
			}
		}
		else
		{
			if (QWEQ(token, curr->token))
			{
				if (curr->broken)
				{
					mur_move_curr_to_broken(ctl, curr);
					--ctl->broken_entries;
				}
				else
					mur_delete_current(ctl, pini_addr);
			}
			else
			{
				mur_move_curr_to_broken(ctl, curr);
				mur_cre_broken(ctl, pini_addr, token);
				proceed = mur_report_error(ctl, MUR_BRKTRANS);
				if (mur_options.rollback)
				{
					--mur_error_count;
					proceed = TRUE;
				}
			}
		}

		if (mur_options.lookback_opers_specified)
			++ctl->lookback_count;

		if (mur_options.rollback  &&  QWNE(seq_num_minus_one, min_epoch_jnl_seqno))
		{
			assert(QWLT(rec->val.jrec_tkill.jnl_seqno, max_epoch_jnl_seqno));
			if (QWGE(rec->val.jrec_tkill.jnl_seqno, consist_jnl_seqno))
			{
				QWSUB(tempqw_seqno, rec->val.jrec_tkill.jnl_seqno, consist_jnl_seqno);
				DWASSIGNQW(tempdw_seqno, tempqw_seqno);
				broken_array[tempdw_seqno].count = -1;
			}
					/* littleton changes -- this doesn't take care of multiple files having the same jnlseqno */
		}
		break;

	case JRT_GSET:
	case JRT_USET:
	case JRT_GKILL:
	case JRT_UKILL:
	case JRT_GZKILL:
	case JRT_UZKILL:
		ASSERT_MATCH(rec->val.jrec_gset, rec->val.jrec_uset);
		ASSERT_MATCH(rec->val.jrec_gset, rec->val.jrec_gkill);
		ASSERT_MATCH(rec->val.jrec_gset, rec->val.jrec_ukill);
		ASSERT_MATCH(rec->val.jrec_gset, rec->val.jrec_gzkill);
		ASSERT_MATCH(rec->val.jrec_gset, rec->val.jrec_uzkill);
		assert(&rec->val.jrec_gset.token ==  &rec->val.jrec_uset.token);
		assert(&rec->val.jrec_gset.token ==  &rec->val.jrec_gkill.token);
		assert(&rec->val.jrec_gset.token ==  &rec->val.jrec_ukill.token);
		assert(&rec->val.jrec_gset.token ==  &rec->val.jrec_gzkill.token);
		assert(&rec->val.jrec_gset.token ==  &rec->val.jrec_uzkill.token);
		assert(&rec->val.jrec_gset.jnl_seqno ==  &rec->val.jrec_uset.jnl_seqno);
		assert(&rec->val.jrec_gset.jnl_seqno ==  &rec->val.jrec_gkill.jnl_seqno);
		assert(&rec->val.jrec_gset.jnl_seqno ==  &rec->val.jrec_ukill.jnl_seqno);
		assert(&rec->val.jrec_gset.jnl_seqno ==  &rec->val.jrec_gzkill.jnl_seqno);
		assert(&rec->val.jrec_gset.jnl_seqno ==  &rec->val.jrec_uzkill.jnl_seqno);

		rec_time = rec->val.jrec_gset.short_time;

		if (mur_options.fences == FENCE_NONE  &&
			(REF_CHAR(&rec->jrec_type) == JRT_GSET  ||  REF_CHAR(&rec->jrec_type) == JRT_GKILL
								||  REF_CHAR(&rec->jrec_type) == JRT_GZKILL))
			break;

		pini_addr = rec->val.jrec_gset.pini_addr;

		if ((pv = mur_get_pini_jpv(ctl, pini_addr)) == NULL)
			proceed = mur_report_error(ctl, MUR_NOPINI);
		else
		{
			QWASSIGN(token, rec->val.jrec_gset.token);
			assert(QWEQ(token, rec->val.jrec_uset.token));
			assert(QWEQ(token, rec->val.jrec_gkill.token));
			assert(QWEQ(token, rec->val.jrec_ukill.token));
			assert(QWEQ(token, rec->val.jrec_gzkill.token));
			assert(QWEQ(token, rec->val.jrec_uzkill.token));

			if ((curr = mur_lookup_current(ctl, pini_addr)) == NULL)
			{
				/* (Use of assignment in the following condition is intentional) */
				if (broken = (!mur_options.selection  ||  mur_do_record(ctl)))
				{
					++ctl->broken_entries;
					proceed = mur_report_error(ctl, MUR_BRKTRANS);
					if (mur_options.rollback)
					{
						--mur_error_count;
						proceed = TRUE;
					}
				}

				mur_cre_current(ctl, pini_addr, token, pv, broken);
			}
			else
			{
				if (QWNE(token, curr->token))
				{
					mur_move_curr_to_broken(ctl, curr);
					mur_cre_current(ctl, pini_addr, token, pv, TRUE);
					proceed = mur_report_error(ctl, MUR_BRKTRANS);
					if (mur_options.rollback)
					{
						--mur_error_count;
						proceed = TRUE;
					}
				}
			}
		}

		if (mur_options.lookback_opers_specified)
			++ctl->lookback_count;
		break;

	case JRT_TCOM:
	case JRT_ZTCOM:


		rec_time = rec->val.jrec_tcom.tc_short_time;
		assert(rec_time == rec->val.jrec_ztcom.tc_short_time);
		assert(&rec->val.jrec_tcom.tc_short_time == &rec->val.jrec_ztcom.tc_short_time);
		assert(&rec->val.jrec_tcom.token ==  &rec->val.jrec_ztcom.token);
		assert(&rec->val.jrec_tcom.jnl_seqno ==  &rec->val.jrec_ztcom.jnl_seqno);

		if (mur_options.fences == FENCE_NONE  &&  REF_CHAR(&rec->jrec_type) == JRT_ZTCOM)
			break;

		pini_addr = rec->val.jrec_tcom.pini_addr;
		assert(pini_addr == rec->val.jrec_ztcom.pini_addr);

		if (mur_lookup_current(ctl, pini_addr) != NULL)
			/* GKILL/GZKILL/GSET/ZTCOM or UKILL/UZKILL/USET/TCOM entries were seen
			   without a corresponding FKILL/FZKILL/FSET or TKILL/TZKILL/TSET */
			GTMASSERT;

		if ((pv = mur_get_pini_jpv(ctl, pini_addr)) == NULL)
		{
			proceed = mur_report_error(ctl, MUR_NOPINI);
			break;
		}

		QWASSIGN(token, rec->val.jrec_tcom.token);
		assert(QWEQ(token, rec->val.jrec_ztcom.token));

		if (!mur_options.before  ||  rec_time <= JNL_M_TIME(before_time))
			mur_cre_current(ctl, pini_addr, token, pv, FALSE);

		if (1 < rec->val.jrec_tcom.participants  &&  participants < rec->val.jrec_tcom.participants)
			participants = rec->val.jrec_tcom.participants;

		if (rec->val.jrec_tcom.participants > 1 && (-1 == mur_decrement_multi(pv->jpv_pid, token)) && !mur_options.rollback)
			mur_cre_multi(pv->jpv_pid, token, rec->val.jrec_tcom.participants - 1, rec_time);

		if ((rec->val.jrec_tcom.participants > 1) && (-1 == mur_decrement_multi_seqno(rec->val.jrec_tcom.jnl_seqno))
				&& mur_options.rollback  &&  QWGE(rec->val.jrec_tcom.jnl_seqno, consist_jnl_seqno))
			mur_cre_multi_seqno(rec->val.jrec_tcom.participants - 1, rec->val.jrec_tcom.jnl_seqno);
		break;
	}

	if (!mur_options.rollback && mur_options.show  &&
		(JRT_ALIGN == REF_CHAR(&rec->jrec_type)  ||  JRT_NULL == REF_CHAR(&rec->jrec_type)  ||
			((!mur_options.before  ||  rec_time <= JNL_M_TIME(before_time))  &&
			 (!mur_options.since  ||  rec_time >= JNL_M_TIME(since_time)))))
		mur_do_show(ctl);

	return proceed;

}
