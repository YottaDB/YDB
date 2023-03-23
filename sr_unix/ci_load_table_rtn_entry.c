/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* Code in this module is based on sr_unix/gtmci.c hence has an FIS copyright
 * even though FIS did not create this module.
 */
#include "mdef.h"

#include "gtm_string.h"

#include "lv_val.h"
#include "fgncal.h"

/* Code used by both ydb_ci_exec() and ydb_ci_get_info() to load/find a call-in table entry
 *
 * Parameters:
 *  - cirtnname	- Address of name of routine
 *  - citab	- Address of pointer to current table description (pointer is reset)
 * Return value:
 *  - Address of entry descriptor for cirtnname
*/
callin_entry_list *ci_load_table_rtn_entry(const char *crtnname, ci_tab_entry_t **citab)
{
	callin_entry_list	*cientry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Validate we have a routine name to drive */
	if ((NULL == crtnname) || ('\0' == *crtnname))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_CIRCALLNAME);
	/* Check if the currently active call-in table is already loaded. If so, use it. */
	*citab = TREF(ci_table_curr);
	if (NULL == *citab)
	{	/* There is no currently active call-in table. Use the default call-in table if it has been loaded. */
		*citab = TREF(ci_table_default);
	}
	if (NULL == *citab)
	{	/* Neither the active nor the default call-in table is available. Load the default call-in table. */
		*citab = ci_tab_entry_open(INTERNAL_USE_FALSE, NULL);
		TREF(ci_table_curr) = *citab;
		TREF(ci_table_default) = *citab;
	}
	if (!(cientry = ci_find_rtn_entry(*citab, crtnname)))   /* Error if rtnname not found in the table (assignment) */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_CINOENTRY, 3, LEN_AND_STR(crtnname), (*citab)->fname);
	return cientry;
}
