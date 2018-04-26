/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "rtnhdr.h"
#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"
#include "iosp.h"
#include "io.h"
#include "error.h"
#include "gtmmsg.h"
#include "ydb_getenv.h"

#define ERR_FGNSYM				\
{						\
	fgn_closepak(handle, INFO);		\
	return FALSE;				\
}

error_def(ERR_COLLFNMISSING);

/* Maps all the collation related symbols from the act shared library.
 * Return TRUE/FALSE based on mapping success.
 */
boolean_t map_collseq(int act, collseq *ret_collseq)
{
	int    		status;
	char		*envptr;
	void_ptr_t 	handle;
	boolean_t	coll_lib_found;
	mstr		tmpmst;
	char		tmpbuff[32];	/* 32 bytes should be a lot more to hold the string representation of an integer "act" */
	static MSTR_CONST(xform_sym_1, "gtm_ac_xform_1");
	static MSTR_CONST(xback_sym_1, "gtm_ac_xback_1");
	static MSTR_CONST(xform_sym, "gtm_ac_xform");
	static MSTR_CONST(xback_sym, "gtm_ac_xback");
	static MSTR_CONST(verify_sym, "gtm_ac_verify");
	static MSTR_CONST(version_sym, "gtm_ac_version");
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	coll_lib_found = FALSE;
	tmpmst.len = SNPRINTF(tmpbuff, SIZEOF(tmpbuff), "%d", act);
	if ((0 > tmpmst.len) || (SIZEOF(tmpbuff) <= tmpmst.len))
	{
		assert(FALSE);
		return FALSE;	/* SNPRINTF returned error or truncated result */
	}
	tmpmst.addr = tmpbuff;
	envptr = ydb_getenv(YDBENVINDX_COLLATE_PREFIX, &tmpmst, NULL_IS_YDB_ENV_MATCH);
	if (NULL == envptr)
		return FALSE;
	if (NULL == (handle = fgn_getpak(envptr, INFO)))
		return FALSE;
	if ((ret_collseq->xform = fgn_getrtn(handle, &xform_sym_1, SUCCESS)))
	{
		if ((ret_collseq->xback = fgn_getrtn(handle, &xback_sym_1, SUCCESS)))
		{
			coll_lib_found = TRUE;
			ret_collseq->argtype = 1;
		} else
		{
			if (!TREF(skip_gtm_putmsg))
			{	/* Warn about the missing routine */
				gtm_putmsg_csa(CSA_ARG(NULL)
					VARLSTCNT(5) ERR_COLLFNMISSING, 3, LEN_AND_LIT("gtm_ac_xback_1()"), ret_collseq->act);
			}
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
			} else
			{
				if (!TREF(skip_gtm_putmsg))
				{	/* Warn about the missing routine */
					gtm_putmsg_csa(CSA_ARG(NULL)
						VARLSTCNT(5) ERR_COLLFNMISSING, 3, LEN_AND_LIT("gtm_ac_xback()"), ret_collseq->act);
				}
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
