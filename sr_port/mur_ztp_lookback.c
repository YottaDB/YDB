/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "iosp.h"
#include "jnl_typedef.h"
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */

GBLREF	int			mur_regno;
GBLREF reg_ctl_list		*mur_ctl;
GBLREF jnl_ctl_list		*mur_jctl;
GBLREF mur_gbls_t		murgbl;
GBLREF mur_opt_struct 		mur_options;
GBLREF mur_rab_t		mur_rab;

LITREF int			jrt_update[];

boolean_t mur_ztp_lookback(void)
{
	multi_struct		*multi;
	reg_ctl_list		*rctl, *rctl_top;
	uint4			rec_pid, status;
	int4			rec_image_count = 0;	/* This is a dummy variable for UNIX */
	ht_entry		*htentry;
	jnl_record		*jrec;
	pini_list_struct	*plst;
	enum jnl_record_type 	rectype;

	error_def(ERR_NOPREVLINK);
	error_def(ERR_MUJNINFO);

	assert(FENCE_NONE != mur_options.fences);
	for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, mur_regno++)
	{
		mur_jctl = rctl->jctl;
		assert(NULL != rctl->jctl_turn_around);
		assert(mur_jctl == rctl->jctl_turn_around);
		assert(mur_jctl->rec_offset == mur_jctl->turn_around_offset);
		status = mur_prev(mur_jctl->rec_offset);
		assert(SS_NORMAL == status);
		for ( ; SS_NORMAL == status; )
		{
			status = mur_prev_rec();	/* scan journal file in reverse order */
			if (SS_NORMAL != status)
				break;
			jrec = mur_rab.jnlrec;
			rectype = mur_rab.jnlrec->prefix.jrec_type;
			if (mur_options.lookback_time_specified && jrec->prefix.time <= mur_options.lookback_time)
				break;
			if (mur_options.lookback_opers_specified)
			{
				if (IS_FUPD_TUPD(rectype) || (!IS_FENCED(rectype) && IS_SET_KILL_ZKILL(rectype)))
				{
					rctl->lookback_count++;
					if (rctl->lookback_count > mur_options.lookback_opers)
						break;
				}
			}
			if (IS_FUPD(rectype))
			{
				status = mur_get_pini(jrec->prefix.pini_addr, &plst);
				if (SS_NORMAL != status)
					break;
				rec_pid = plst->jpv.jpv_pid;
				VMS_ONLY(rec_image_count = plst->jpv.jpv_image_count;)
			    	if ((NULL != (multi = MUR_TOKEN_LOOKUP(((struct_jrec_ztp_upd *)jrec)->token, rec_pid,
								rec_image_count, 0, ZTPFENCE))) && (0 < multi->partner))
				{	/* this transaction has already been identified as broken */
					assert(murgbl.tp_resolve_time >= jrec->prefix.time);
					murgbl.tp_resolve_time = jrec->prefix.time;
					/* see turn-around-point code in mur_back_process/mur_apply_pblk for the fields to clear */
					rctl->jctl_turn_around->turn_around_offset = 0;
					rctl->jctl_turn_around->turn_around_time = 0;
					rctl->jctl_turn_around->turn_around_seqno = 0;
					rctl->jctl_turn_around->turn_around_tn = 0;
					/* Now save new turn around point.
					 * Force the entire Z transaction beginning from ZTSTART (FUPD actually)
					 * to be reported as broken and undo this transaction from the db */
					rctl->jctl_turn_around = mur_jctl;
					mur_jctl->turn_around_offset = mur_jctl->rec_offset;
					if (multi->time > jrec->prefix.time)
						multi->time = jrec->prefix.time;
					if (mur_options.verbose)
						gtm_putmsg(VARLSTCNT(15) ERR_MUJNINFO, 13, LEN_AND_LIT("Mur_ztp_lookback"),
						mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset,
						murgbl.tp_resolve_time, mur_jctl->turn_around_offset, mur_jctl->turn_around_time,
						mur_jctl->turn_around_tn, &mur_jctl->turn_around_seqno,
						0, murgbl.token_table.count, murgbl.broken_cnt);
				}
			}
		}
		if (ERR_NOPREVLINK == status)
		{
			if (mur_options.lookback_time_specified)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_NOPREVLINK, 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
				return FALSE;
			}
			gtm_putmsg(VARLSTCNT(4) MAKE_MSG_INFO(ERR_NOPREVLINK), 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
		} else if (SS_NORMAL != status)
			return FALSE;
	}
	return TRUE;
}
