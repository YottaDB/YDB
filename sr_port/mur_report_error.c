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

#include "gtm_string.h"

#include "gdsroot.h"
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
#include "copy.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF	mur_rab_t	mur_rab;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF 	int		mur_regno;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF 	mur_gbls_t	murgbl;

boolean_t mur_report_error(enum mur_error code)
{
	error_def(ERR_UNKNOWNRECTYPE);
	error_def(ERR_DUPTOKEN);
	error_def(ERR_PREVJNLNOEOF);
	error_def(ERR_JNLBADRECFMT);

	switch (code)
	{
	default:
		assert(FALSE);
		break;

	case MUR_DUPTOKEN:
		assert(FALSE);
 		gtm_putmsg(VARLSTCNT(7) ERR_DUPTOKEN, 5, &((struct_jrec_tcom *)mur_rab.jnlrec)->token_seq.token,
 				mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, DB_LEN_STR(mur_ctl[mur_regno].gd));
		break;

	case MUR_PREVJNLNOEOF:
		gtm_putmsg(VARLSTCNT(4) ERR_PREVJNLNOEOF, 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
		break;

	case MUR_JNLBADRECFMT:
		gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset);
		break;

	}
	return MUR_WITHIN_ERROR_LIMIT(murgbl.err_cnt, mur_options.error_limit); /* side-effect : increments murgbl.err_cnt */
}
