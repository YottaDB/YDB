/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "libyottadb_int.h"
#include "gvsub2str.h"
#include "stringpool.h"

GBLREF gv_key		*gv_altkey;

/* Code in this module is based on format_targ_key.c and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

/* Routine to extract subscripts from gv_altkey and store references to those subscripts in TREF(sapi_query_node_subs) */
void sapi_save_targ_key_subscr_nodes(void)
{
	mstr		*subcur, opstr;
	unsigned char	*gvkey_char_ptr, *gvkey_top_ptr, work_buff[MAX_ZWR_KEY_SZ], *work_top;
	boolean_t	is_string;
	int		subs_cnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 == TREF(sapi_query_node_subs_cnt));	/* Caller should have initialized */
	/* Verify global subscript array is available and if not make it so */
	if (NULL == TREF(sapi_query_node_subs))
		TREF(sapi_query_node_subs) = malloc(SIZEOF(mstr) * YDB_MAX_SUBS);	/* Allocate mstr array we need */
	subcur = TREF(sapi_query_node_subs);
	gvkey_char_ptr = gv_altkey->base;
	gvkey_top_ptr = gvkey_char_ptr + gv_altkey->end;
	/* Ensure input key is well-formed (i.e. double null terminated) */
	assert(KEY_DELIMITER == *(gvkey_top_ptr - 1));
	assert(KEY_DELIMITER == *gvkey_top_ptr);
	/* Find end of global name and start of subscripts */
	for (; ('\0' != *gvkey_char_ptr++) && (gvkey_char_ptr <= gvkey_top_ptr);)
		;
	/* The following asserts (in the for loop) assume that a global name will be able to fit in completely into any key. */
	assert(gvkey_char_ptr <= gvkey_top_ptr);
	subs_cnt = 0;
	for ( ; gvkey_char_ptr < gvkey_top_ptr; )
	{
		if (SUBSCRIPT_STDCOL_NULL == *gvkey_char_ptr)	/* This is a null string in Standard Null Collation format */
		{
			subcur->addr = NULL;
			subcur->len = 0;
			gvkey_char_ptr++;
			assert(KEY_DELIMITER == *gvkey_char_ptr);
			gvkey_char_ptr++;
		} else
		{
			opstr.addr = (char *)work_buff;
			opstr.len = MAX_ZWR_KEY_SZ;
			work_top = gvsub2str(gvkey_char_ptr, &opstr, FALSE);
			subcur->addr = (char *)work_buff;
			subcur->len = work_top - work_buff;
			s2pool(subcur);		/* Rebuffer in stringpool so value survives return from this routine */
			/* Advance until next null separator */
			for ( ; *gvkey_char_ptr++ && (gvkey_char_ptr <= gvkey_top_ptr); )
				;
		}
		assert(gvkey_char_ptr <= gvkey_top_ptr);
		subcur++;
		subs_cnt++;
		TREF(sapi_query_node_subs_cnt) = subs_cnt;
	}
	assert(!subs_cnt || (TREF(sapi_query_node_subs_cnt) == subs_cnt));
	if (!subs_cnt)
	{	/* Returning just the name (no subscripts) so set the subscript count to -1 to
		 * differentiate it from a subs_cnt=0 return which signals the end of the list (YDB_ERR_NODEEND).
		 * Similar code exists in "sr_port/op_fnreversequery.c" for local variables.
		 */
		TREF(sapi_query_node_subs_cnt) = -1;
	}
	return;
}
