/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
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
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "jnl_typedef.h"
#include "gtmmsg.h"			/* for gtm_putmsg() prototype */
#include "mur_validate_checksum.h"	/* for "mur_validate_checksum" */

GBLREF reg_ctl_list		*mur_ctl;
GBLREF mur_gbls_t		murgbl;
GBLREF mur_opt_struct 		mur_options;

boolean_t mur_ztp_lookback(void)
{
	multi_struct		*multi;
	reg_ctl_list		*rctl, *rctl_top;
	jnl_ctl_list		*jctl;
	uint4			status;
	int4			rec_image_count = 0;	/* This is a dummy variable for UNIX */
	jnl_record		*jrec;
	pini_list_struct	*plst;
	enum jnl_record_type 	rectype;
	token_num		token;

	error_def(ERR_NOPREVLINK);
	error_def(ERR_MUINFOUINT4);
	error_def(ERR_MUINFOUINT8);
	error_def(ERR_MUINFOSTR);
	error_def(ERR_TEXT);

	assert(FENCE_NONE != mur_options.fences);
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		jctl = rctl->jctl;
		assert(jctl->reg_ctl == rctl);
		assert(NULL != rctl->jctl_turn_around);
		assert(jctl == rctl->jctl_turn_around);
		assert(jctl->rec_offset == jctl->turn_around_offset);
		status = mur_prev(jctl, jctl->rec_offset);
		assert(SS_NORMAL == status);
		for ( ; SS_NORMAL == status; )
		{
			status = mur_prev_rec(&jctl);	/* scan journal file in reverse order */
			if (SS_NORMAL != status)
				break;
			jrec = jctl->reg_ctl->mur_desc->jnlrec;
			rectype = (enum jnl_record_type)jrec->prefix.jrec_type;
			if (mur_options.verify && !mur_validate_checksum(jctl))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Checksum validation failed"));
				return FALSE;
			}
			if (mur_options.lookback_time_specified && jrec->prefix.time <= mur_options.lookback_time)
				break;
			if (mur_options.lookback_opers_specified)
			{
				if (IS_FUPD_TUPD(rectype) || (!IS_FENCED(rectype) && IS_SET_KILL_ZKILL_ZTRIG(rectype)))
				{
					rctl->lookback_count++;
					if (rctl->lookback_count > mur_options.lookback_opers)
						break;
				}
			}
			if (IS_FUPD(rectype))
			{
				VMS_ONLY(
					MUR_GET_IMAGE_COUNT(jctl, jrec, rec_image_count, status);
					if (SS_NORMAL != status)
						break;
				)
				token = ((struct_jrec_upd *)jrec)->token_seq.token;
			    	if ((NULL != (multi = MUR_TOKEN_LOOKUP(token, rec_image_count, 0, ZTPFENCE)))
								&& (0 < multi->partner))
				{	/* this transaction has already been identified as broken */
					/* see turn-around-point code in mur_back_process/mur_apply_pblk for the fields to clear */
					rctl->jctl_turn_around->turn_around_offset = 0;
					rctl->jctl_turn_around->turn_around_time = 0;
					rctl->jctl_turn_around->turn_around_seqno = 0;
					rctl->jctl_turn_around->turn_around_tn = 0;
					/* Now save new turn around point.
					 * Force the entire Z transaction beginning from ZTSTART (FUPD actually)
					 * to be reported as broken and undo this transaction from the db */
					rctl->jctl_turn_around = jctl;
					jctl->turn_around_offset = jctl->rec_offset;
					if (multi->time > jrec->prefix.time)
						multi->time = jrec->prefix.time;
					if (mur_options.verbose)
						PRINT_VERBOSE_STAT(jctl, "mur_ztp_lookback");
				}
			}
		}
		if (ERR_NOPREVLINK == status)
		{
			if (mur_options.lookback_time_specified)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_NOPREVLINK, 2, jctl->jnl_fn_len, jctl->jnl_fn);
				return FALSE;
			}
			gtm_putmsg(VARLSTCNT(4) MAKE_MSG_INFO(ERR_NOPREVLINK), 2, jctl->jnl_fn_len, jctl->jnl_fn);
		} else if (SS_NORMAL != status)
			return FALSE;
	}
	return TRUE;
}
