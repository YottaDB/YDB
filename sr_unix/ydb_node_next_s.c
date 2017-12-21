/****************************************************************
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "lv_val.h"
#include "hashtab_mname.h"
#include "callg.h"
#include "op.h"
#include "error.h"
#include "nametabtyp.h"
#include "namelook.h"
#include "stringpool.h"
#include "libyottadb_int.h"
#include "libydberrors.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

/* */
int ydb_node_next_s(ydb_buffer_t *varname, int subs_used, ydb_buffer_t *subsarray,
		    int *ret_subs_used, ydb_buffer_t *ret_subsarray)
{
	boolean_t	error_encountered;
	int		get_svn_index;
	ydb_var_types	get_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify entry conditions, make sure YDB CI environment is up etc. */
	LIBYOTTADB_INIT(LYDB_RTN_NODE_NEXT);	/* Note: macro could "return" from this function in case of errors */
	TREF(sapi_mstrs_for_gc_indx) = 0;			/* Clear any previously used entries */
	ESTABLISH_NORET(ydb_simpleapi_ch, error_encountered);
	if (error_encountered)
	{
		assert(0 == TREF(sapi_mstrs_for_gc_indx));	/* should have been cleared by "ydb_simpleapi_ch" */
		REVERT;
		return ((ERR_TPRETRY == SIGNAL) ? YDB_TP_RESTART : -(TREF(ydb_error_code)));
	}
	/* Do some validation */
	VALIDATE_VARNAME(varname, get_type, get_svn_index, FALSE);
	if (0 > subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_VARNAMEINVALID);
	if (YDB_MAX_SUBS < subs_used)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
	if ((0 == *ret_subs_used) || (NULL == ret_subsarray))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NORETBUFFER, 2, RTS_ERROR_LITERAL("ydb_node_next_s()"));
	return 0;
}
