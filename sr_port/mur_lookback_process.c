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

GBLREF	mur_opt_struct	mur_options;



/*
 *	This routine is a stripped-down version of mur_back_process().
 *	It is used only to try to resolve any multi-region transactions
 *	that remain incompletely accounted for after mur_back_process()
 *	has reached the nominal "turn-around" point in each of the journal
 *	files.  Essentially, this routine looks for TCOM or ZTCOM records
 *	prior to the turn-around point backwards to the LOOKBACK_LIMIT.
 *
 *	It returns TRUE for all record types until it encounters the most
 *	recent EPOCH record prior to the LOOKBACK_LIMIT.
 */

bool	mur_lookback_process(ctl_list *ctl)
{
	bool			eof;
	jnl_record		*rec;
	jnl_process_vector	*pv;


	rec = (jnl_record *)ctl->rab->recbuff;

	switch (REF_CHAR(&rec->jrec_type))
	{
	default:

		return mur_report_error(ctl, MUR_UNKNOWN);


	case JRT_PINI:
	case JRT_PFIN:
	case JRT_PBLK:
	case JRT_ALIGN:

		return TRUE;


	case JRT_EOF:

		eof = ctl->found_eof;
		ctl->found_eof = TRUE;

		return !eof  ||  mur_report_error(ctl, MUR_MULTEOF);


	case JRT_EPOCH:

		ctl->reached_lookback_limit = rec->val.jrec_epoch.short_time < JNL_M_TIME(lookback_time)  ||
					      ctl->lookback_count > mur_options.lookback_opers;

		return !ctl->reached_lookback_limit;


	case JRT_SET:
	case JRT_FSET:
	case JRT_GSET:
	case JRT_TSET:
	case JRT_USET:
	case JRT_KILL:
	case JRT_FKILL:
	case JRT_GKILL:
	case JRT_TKILL:
	case JRT_UKILL:
	case JRT_NULL:
	case JRT_ZKILL:
	case JRT_FZKILL:
	case JRT_GZKILL:
	case JRT_TZKILL:
	case JRT_UZKILL:
	case JRT_INCTN:
	case JRT_AIMG:

		if (mur_options.lookback_opers_specified)
			++ctl->lookback_count;

		return TRUE;


	case JRT_TCOM:
	case JRT_ZTCOM:

		if ((pv = mur_get_pini_jpv(ctl, rec->val.jrec_tcom.pini_addr)) != NULL)
			mur_decrement_multi(pv->jpv_pid, rec->val.jrec_tcom.token);

		return TRUE;
	}

}
