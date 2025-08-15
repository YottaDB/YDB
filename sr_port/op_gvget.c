/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "gvcst_protos.h"	/* for gvcst_get prototype */
#include "gvcmx.h"
#include "sgnl.h"
#include "op.h"
#include "libyottadb_int.h"

GBLREF	gv_namehead	*gv_target;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;
GBLREF	bool		undef_inhibit;

LITREF	mval		literal_null;

#define	NONULLSUBS	"$GET() failed because"

/* From the generated code's point of view, this is a void function.
 * The defined state isn't looked at by MUMPS code.  This routine,
 * however, is used by some servers who would dearly like to know
 * the status of the get, though they don't want to cause any
 * errors.
 */
boolean_t op_gvget(mval *v)
{
	boolean_t	 gotit;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(gv_last_subsc_null) && NEVER == gv_cur_region->null_subs)
		sgnl_gvnulsubsc(NONULLSUBS);
	if (IS_REG_BG_OR_MM(gv_cur_region))
	{
	 	if (0 == gv_target->root)		/* global does not exist */
		{	/* Assert that if ydb_gvundef_fatal is non-zero, then we better not be about to signal a GVUNDEF */
			assert(!TREF(ydb_gvundef_fatal));
			gotit = FALSE;
		} else
		{
			DEBUG_ONLY(TREF(in_op_gvget) = TRUE;)
		 	gotit = gvcst_get(v);
			assert(FALSE == TREF(in_op_gvget)); /* gvcst_get should have reset it right away */
		}
	} else
	{
		assert(REG_ACC_METH(gv_cur_region) == dba_cm);
	 	gotit = gvcmx_get(v);
	}
	if (!gotit)
	{
		if (undef_inhibit)
			*v = literal_null;
		else if (LYDB_RTN_GET == TREF(libyottadb_active_rtn))
		{	/* This is a "ydb_get_s()" or "ydb_get_st()" call. Do not issue a GVUNDEF error.
			 * Instead just set TREF(ydb_error_code) so caller ydb_get_s() can return YDB_ERR_GVUNDEF.
			 * This avoids heavyweight error processing. (YDB#1164).
			 */
			TREF(ydb_error_code) = ERR_GVUNDEF;
		} else
			sgnl_gvundef();	/* Issue GVUNDEF error */
	}
	return gotit;
}
