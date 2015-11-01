/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdlib.h>

#include "collseq.h"
#include "rtnhdr.h"
#include "fgncal.h"
#include "iosp.h"
#include "io.h"
#include "trans_log_name.h"
#include "error.h"

#define ERR_FGNSYM				\
{						\
	fgn_closepak(handle, INFO);		\
	return FALSE;				\
}

/* Maps all the collation related symbols from the act shared library.
 * Return TRUE/FALSE based on mapping success.
 */

boolean_t map_collseq(mstr *fspec, collseq *ret_collseq)
{
	mstr   		fspec_trans;
	char   		buffer[MAX_TRANS_NAME_LEN];
	int    		status;
	void_ptr_t 	handle;

	static MSTR_CONST(xform_sym, "gtm_ac_xform");
	static MSTR_CONST(xback_sym, "gtm_ac_xback");
	static MSTR_CONST(verify_sym, "gtm_ac_verify");
	static MSTR_CONST(version_sym, "gtm_ac_version");

	if (SS_NORMAL != (status = trans_log_name(fspec, &fspec_trans, buffer)))
    	    return FALSE;

	if (NULL == (handle = fgn_getpak(buffer, INFO)))
    	    return FALSE;

	if (!(ret_collseq->xform = fgn_getrtn(handle, &xform_sym, INFO)))
		ERR_FGNSYM;
	if (!(ret_collseq->xback = fgn_getrtn(handle, &xback_sym, INFO)))
		ERR_FGNSYM;
	if (!(ret_collseq->verify = fgn_getrtn(handle, &verify_sym, INFO)))
		ERR_FGNSYM;
	if (!(ret_collseq->version = fgn_getrtn(handle, &version_sym, INFO)))
		ERR_FGNSYM;

	return TRUE;
}
