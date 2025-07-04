/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "op.h"
#include "gvcst_protos.h"	/* for gvcst_query prototype */
#include "format_targ_key.h"
#include "gvcmx.h"
#include "libyottadb_int.h"
#include "gvt_inline.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey, *gv_altkey;
GBLREF spdesc		stringpool;
GBLREF gd_region	*gv_cur_region;
GBLREF mstr             extnam_str;

/* This function is the entry point for $query(gvn) or $query(gvn,1) that indicates a "formward" query.
 * "gv_currkey" already points to the input "gvn".
 * "v" stores the result of the reverse $query at function end.
 *
 * Note - this module runs in two modes:
 *   1. Normal calls from generated code on behalf of a $QUERY() usage.
 *   2. Calls to ydb_node_next_s() from the SimpleAPI.
 * The IS_SIMPLEAPI_MODE macro can determine which mode we are in. The mode regulates how this routine returns its
 * output to the caller. The simpleAPI "returns" its information into a static array allocated at need with the addr
 * cached in TREF(sapi_query_node_subs) with the count of valid entries in TREF(sapi_query_node_subs_cnt) while a
 * YDB runtime call returns a string in the dst mval.
 */
void op_gvquery(mval *v)
{
	int4			size;
	unsigned char		buff[MAX_ZWR_KEY_SZ], *end, *glob_begin;
 	boolean_t		currkey_has_special_meaning, found, last_subsc_is_null;
	boolean_t		is_simpleapi_mode;
	enum db_acc_method 	acc_meth;
	unsigned char		ch1, ch2, *extnamsrc, *extnamdst, *extnamtop;
	int			maxlen;
	char			extnamdelim[] = "^|\"\"|";
	mval			val;
	gv_key			*last_gvquery_key;
	gvnh_reg_t		*gvnh_reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(sapi_query_node_subs_cnt) = 0;
	last_gvquery_key = TREF(last_gvquery_key);
	if (NULL == last_gvquery_key)
	{
		GVKEY_INIT(last_gvquery_key, DBKEYSIZE(MAX_KEY_SZ));
		TREF(last_gvquery_key) = last_gvquery_key;
	}
	acc_meth = REG_ACC_METH(gv_cur_region);
	/* Modify gv_currkey such that a gvcst_search of the resulting key will find the next available record in collation order */
	assert(dba_usr != acc_meth);
	if (TREF(gv_last_subsc_null) && (NEVER == gv_cur_region->std_null_coll))
	{	/* Treat null subscript specification as a special meaning (to get the first subscript) */
		if ((last_gvquery_key->end != gv_currkey->end)
		    || memcmp(last_gvquery_key->base, gv_currkey->base, last_gvquery_key->end))
		{
			currkey_has_special_meaning = TRUE;
			assert(STR_SUB_PREFIX == gv_currkey->base[gv_currkey->prev]);
			gv_currkey->base[gv_currkey->prev] = 01;
		} else
			currkey_has_special_meaning = FALSE;
	} else
		currkey_has_special_meaning = FALSE;
	if (!currkey_has_special_meaning)
	{	/* Input key is to be treated as is. No special meaning like is the case for a null subscript.
		 * Note, gv_currkey->prev isn't changed here. We rely on this in gtcmtr_query to distinguish
		 * different forms of the key.
		 */
		gv_currkey->base[gv_currkey->end++]= 1;
		gv_currkey->base[gv_currkey->end++] = KEY_DELIMITER;
		gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
	}
	if (IS_ACC_METH_BG_OR_MM(acc_meth))
	{
		gvnh_reg = TREF(gd_targ_gvnh_reg);
		if (NULL == gvnh_reg)
			found = ((0 != gv_target->root) ? gvcst_query() : FALSE); /* global does not exist if root is 0 */
		else
			INVOKE_GVCST_SPR_XXX(gvnh_reg, found = gvcst_spr_query());
	} else
	{
		assert(acc_meth == dba_cm);
		found = gvcmx_query(&val); /* val ignored currently - Vinaya Aug 13, 2001*/
	}
	/* Restore gv_currkey to what it was at function entry time */
	if (currkey_has_special_meaning)
	{
		assert(01 == gv_currkey->base[gv_currkey->prev]);
		gv_currkey->base[gv_currkey->prev] = STR_SUB_PREFIX;
	} else
	{
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end]);
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 1]);
		assert(1 == gv_currkey->base[gv_currkey->end - 2]);
		assert(KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 3]);
		gv_currkey->end -= 2;
		gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
	}
	is_simpleapi_mode = IS_SIMPLEAPI_MODE;
	assert((is_simpleapi_mode && (NULL == v)) || (!is_simpleapi_mode && (NULL != v)));
	if (!is_simpleapi_mode)
		v->mvtype = 0;	/* So STP_GCOL (if invoked below) can free up space
				 * currently occupied by this to-be-overwritten mval.
				 */
	if (found)
	{
		/* We have a known access method - save the subscript path appropriately for the mode we are in */
		if (!is_simpleapi_mode)
		{	/* MM/BG mode - do a normal ASCII format of the reference for return */
			if (0 == (end = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gv_altkey, TRUE)))
				end = &buff[MAX_ZWR_KEY_SZ - 1];
			size = (int)(end - &buff[0] - 1);	/* Exclude ^ */
			glob_begin = &buff[1];			/* Skip ^ */
		} else
		{	/* In simpleAPI mode, the subscripts are just stored in the TREF(sapi_query_node_subs) array
			 * which the calling ydb_* routine will use to rebuffer the subscripts into its caller's
			 * output subscript array.
			 */
			sapi_save_targ_key_subscr_nodes();
		}
		if (!is_simpleapi_mode)
		{	/* When returning the "normal" M code global reference string, we need to return a double-quote for
			 * every single-quote; assume worst case. Account for ^ in both cases - extnam and no extnam.
			 */
			maxlen = size + ((0 == extnam_str.len) ? 1 : ((extnam_str.len * 2) + (int)(STR_LIT_LEN(extnamdelim))));
			ENSURE_STP_FREE_SPACE(maxlen);
			extnamdst = stringpool.free;
			*extnamdst++ = extnamdelim[0];
			if (0 < extnam_str.len)
			{
				*extnamdst++ = extnamdelim[1];
				*extnamdst++ = extnamdelim[2];
				for (extnamsrc = (unsigned char *)extnam_str.addr, extnamtop = extnamsrc + extnam_str.len;
				     extnamsrc < extnamtop; )
				{
					*extnamdst++ = *extnamsrc;
					if ('"' == *extnamsrc++)	/* Caution : pointer increment side-effect */
						*extnamdst++ = '"';
				}
				*extnamdst++ = extnamdelim[3];
				*extnamdst++ = extnamdelim[4];
			}
			memcpy(extnamdst, glob_begin, size);
			v->str.len = INTCAST(extnamdst - stringpool.free + size);
			v->str.addr = (char *)stringpool.free;
			stringpool.free += v->str.len;
			assert (v->str.addr < (char *)stringpool.top && v->str.addr >= (char *)stringpool.base);
			assert (v->str.addr + v->str.len <= (char *)stringpool.top &&
				v->str.addr + v->str.len >= (char *)stringpool.base);
			assert(2 <= gv_altkey->end);
		}
		/* Housekeeping for NULL subscript recognition */
		ch1 = gv_altkey->base[gv_altkey->end - 2];
		if ((STR_SUB_PREFIX == ch1) || (SUBSCRIPT_STDCOL_NULL == ch1))
		{
			assert(3 <= gv_altkey->end);
			ch2 = gv_altkey->base[gv_altkey->end - 3];
			last_subsc_is_null = (KEY_DELIMITER == ch2);
		} else
			last_subsc_is_null = FALSE;
		if (last_subsc_is_null)
			COPY_KEY(last_gvquery_key, gv_altkey);
	} else /* !found */
	{
		if (!is_simpleapi_mode)
			v->str.len = 0;
		last_gvquery_key->end = 0;
	}
	if (!is_simpleapi_mode)
		v->mvtype = MV_STR;	/* Initialize mvtype now that mval has been otherwise completely set up */
	return;
}
