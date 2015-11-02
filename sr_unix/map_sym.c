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

#include "gtm_stdlib.h"

#include "collseq.h"
#include <rtnhdr.h>
#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"
#include "iosp.h"
#include "io.h"
#include "trans_log_name.h"
#include "error.h"
#include "gtmmsg.h"

#define ERR_FGNSYM				\
{						\
	fgn_closepak(handle, INFO);		\
	return FALSE;				\
}

error_def(ERR_COLLFNMISSING);

/* Maps all the collation related symbols from the act shared library.
 * Return TRUE/FALSE based on mapping success.
 */
boolean_t map_collseq(mstr *fspec, collseq *ret_collseq)
{
	mstr   		fspec_trans;
	char   		buffer[MAX_TRANS_NAME_LEN];
	int    		status;
	void_ptr_t 	handle;
	boolean_t	coll_lib_found;

	static MSTR_CONST(xform_sym_1, "gtm_ac_xform_1");
	static MSTR_CONST(xback_sym_1, "gtm_ac_xback_1");
	static MSTR_CONST(xform_sym, "gtm_ac_xform");
	static MSTR_CONST(xback_sym, "gtm_ac_xback");
	static MSTR_CONST(verify_sym, "gtm_ac_verify");
	static MSTR_CONST(version_sym, "gtm_ac_version");
	coll_lib_found = FALSE;
	if (SS_NORMAL != (status = TRANS_LOG_NAME(fspec, &fspec_trans, buffer, SIZEOF(buffer), do_sendmsg_on_log2long)))
		return FALSE;
	if (NULL == (handle = fgn_getpak(buffer, INFO)))
		return FALSE;
	if ((ret_collseq->xform = fgn_getrtn(handle, &xform_sym_1, SUCCESS)))
	{
		if ((ret_collseq->xback = fgn_getrtn(handle, &xback_sym_1, SUCCESS)))
		{
			coll_lib_found = TRUE;
			ret_collseq->argtype = 1;
		} else
		{	/* Warn about the missing routine */
			gtm_putmsg(VARLSTCNT(5) ERR_COLLFNMISSING, 3, LEN_AND_LIT("gtm_ac_xback_1()"), ret_collseq->act);
			ERR_FGNSYM;
		}
	}
	if ( FALSE == coll_lib_found)
	{
		if ((ret_collseq->xform = fgn_getrtn(handle, &xform_sym, SUCCESS)))
		{
			if ((ret_collseq->xback = fgn_getrtn(handle, &xback_sym, SUCCESS)))
			{
				coll_lib_found = TRUE;
				ret_collseq->argtype = 0;
			}
			else { /* Warn about the missing routine */
				gtm_putmsg(VARLSTCNT(5) ERR_COLLFNMISSING, 3, LEN_AND_LIT("gtm_ac_xback()"), ret_collseq->act );
				ERR_FGNSYM;
			}
		} else /* Neither xform_1 or xform is found */
			ERR_FGNSYM;
	}
	assert(TRUE == coll_lib_found);
	if (!(ret_collseq->verify = fgn_getrtn(handle, &verify_sym, INFO)))
		ERR_FGNSYM;
	if (!(ret_collseq->version = fgn_getrtn(handle, &version_sym, INFO)))
		ERR_FGNSYM;
	return TRUE;
}
