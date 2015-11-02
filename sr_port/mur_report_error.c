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

#include "gtm_string.h"

#include "gdsroot.h"
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
#include "copy.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF 	mur_gbls_t	murgbl;

boolean_t mur_report_error(jnl_ctl_list *jctl, enum mur_error code)
{
	error_def(ERR_BOVTMGTEOVTM);
	error_def(ERR_DUPTOKEN);
	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_PREVJNLNOEOF);
	error_def(ERR_UNKNOWNRECTYPE);

	switch (code)
	{
	default:
		assert(FALSE);
		break;

	case MUR_DUPTOKEN:
		assert(FALSE);
 		gtm_putmsg(VARLSTCNT(7) ERR_DUPTOKEN, 5, &((struct_jrec_tcom *)jctl->reg_ctl->mur_desc->jnlrec)->token_seq.token,
 				jctl->jnl_fn_len, jctl->jnl_fn, DB_LEN_STR(jctl->reg_ctl->gd));
		break;

	case MUR_PREVJNLNOEOF:
		gtm_putmsg(VARLSTCNT(4) ERR_PREVJNLNOEOF, 2, jctl->jnl_fn_len, jctl->jnl_fn);
		break;

	case MUR_JNLBADRECFMT:
		gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, jctl->jnl_fn_len, jctl->jnl_fn, jctl->rec_offset);
		break;

	case MUR_BOVTMGTEOVTM:
		gtm_putmsg(VARLSTCNT(6) ERR_BOVTMGTEOVTM, 4, jctl->jnl_fn_len, jctl->jnl_fn,
							&jctl->jfh->bov_timestamp, &jctl->jfh->eov_timestamp);
		break;
	}
	return MUR_WITHIN_ERROR_LIMIT(murgbl.err_cnt, mur_options.error_limit); /* side-effect : increments murgbl.err_cnt */
}
