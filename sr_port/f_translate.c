/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "gtm_string.h"
#include "mdef.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "stringpool.h"
#include "min_max.h"
#include "mmemory.h"
#include "op.h"

#ifdef UTF8_SUPPORTED
#include "hashtab_int4.h"
#include "hashtab.h"
#include "gtm_utf8.h"
#endif

LITREF mval literal_null;

int f_translate(oprtype *a, opctype op)
{
	boolean_t	more_args, use_hash = FALSE, clean = FALSE;
	/* srch = searhc string, rplc = replace string */
	int		i, srch_string_len, rplc_str_len, min;
	int4		*xlate, maxLengthString, inlen, outlen, delete_value, no_value;
	unsigned int	code;
	triple		*args[4];
	mval		*xlateTable, *srch_mval, *rplc_mval;
	ht_ent_int4     *tabent;
	char		*srch_string, *rplc_str;
	sm_uc_ptr_t	inpt, outpt, ntop, ch, nextptr, buffer, outtop;
	hash_table_int4 *xlate_hash;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	args[0] = maketriple(op);
	if (EXPR_FAIL == expr(&(args[0]->operand[0]), MUMPS_EXPR))
		return FALSE;
	for (i = 1 , more_args = TRUE ; i < 3 ; i++)
	{
		args[i] = newtriple(OC_PARAMETER);
		if (more_args)
		{
			if (TK_COMMA != TREF(window_token))
				more_args = FALSE;
			else
			{
				advancewindow();
				if (EXPR_FAIL == expr(&(args[i]->operand[0]), MUMPS_EXPR))
					return FALSE;
			}
		}
		if (!more_args)
			args[i]->operand[0] = put_lit((mval *)&literal_null);
		args[i - 1]->operand[1] = put_tref(args[i]);
	}
	assert((TRIP_REF == args[0]->operand[1].oprclass) && (TRIP_REF == args[0]->operand[1].oprval.tref->operand[0].oprclass)
			&& (TRIP_REF == args[0]->operand[1].oprval.tref->operand[1].oprclass));
	/* If the second and third parameters are literals, pre-calculate the translation table and store it in the stringpool */
	if ((OC_LIT == args[1]->operand[0].oprval.tref->opcode)
			&& (OC_LIT == args[2]->operand[0].oprval.tref->opcode))
	{	/* We only do this if we have all literals and (not utf8, or utf8 and valid strings) */
		srch_mval = &args[1]->operand[0].oprval.tref->operand[0].oprval.mlit->v;
		rplc_mval = &args[2]->operand[0].oprval.tref->operand[0].oprval.mlit->v;
		assert(MV_STR & srch_mval->mvtype);
		assert(MV_STR & rplc_mval->mvtype);
		ENSURE_STP_FREE_SPACE(NUM_CHARS * SIZEOF(int4));
		xlate = (int4 *)stringpool.free;
		stringpool.free += NUM_CHARS * SIZEOF(int4);
		if (!gtm_utf8_mode || OC_FNZTRANSLATE == op ||
				(gtm_utf8_mode && valid_utf_string(&srch_mval->str) && valid_utf_string(&rplc_mval->str)))
		{
			unuse_literal(&args[1]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
			dqdel(args[1]->operand[0].oprval.tref, exorder);
			dqdel(args[2]->operand[0].oprval.tref, exorder);
			dqdel(args[1], exorder);
			dqdel(args[2], exorder);
		}
		if (!gtm_utf8_mode || OC_FNZTRANSLATE == op)
		{
			create_byte_xlate_table(srch_mval, rplc_mval, xlate);
			xlateTable = (mval *)mcalloc(SIZEOF(mval));
			xlateTable->str.addr = (char *)xlate;
			xlateTable->mvtype = MV_STR;
			xlateTable->str.len = NUM_CHARS * SIZEOF(int4);
			args[0]->opcode = OC_FNZTRANSLATE_FAST;
			args[0]->operand[1] = put_lit((mval *)xlateTable);
			unuse_literal(&args[2]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
		}
		else if (gtm_utf8_mode && valid_utf_string(&srch_mval->str) && valid_utf_string(&rplc_mval->str))
		{	/* The optimized version needs 4 arguments; the src, rplc, xlate, xlate_hash, so we need to allocate more
		 	 	 triples. We can remove the old arguments (mostly, except the rplc string) */
			args[0]->opcode = OC_FNTRANSLATE_FAST; /* note no Z */
			args[1] = newtriple(OC_PARAMETER);
			/* Promote the rplc string to the second argument */
			args[1]->operand[0] = args[2]->operand[0];
			args[2] = newtriple(OC_PARAMETER);
			args[3] = newtriple(OC_PARAMETER);
			args[0]->operand[1].oprval.tref = newtriple(OC_PARAMETER);
			xlate_hash = create_utf8_xlate_table(srch_mval, rplc_mval, xlate);
			if (NULL != xlate_hash)
			{
				maxLengthString = (xlate_hash->size * SIZEOF(ht_ent_int4))
					+ SIZEOF(hash_table_int4) + ROUND_UP(xlate_hash->size, BITS_PER_UCHAR);
				ENSURE_STP_FREE_SPACE(maxLengthString);
				nextptr = copy_hashtab_to_buffer_int4(xlate_hash, stringpool.free, NULL);
				xlateTable = (mval *)mcalloc(SIZEOF(mval));
				xlateTable->str.addr = (char *)stringpool.free;
				xlateTable->mvtype = MV_STR;
				xlateTable->str.len = nextptr - stringpool.free;
				assert(maxLengthString == xlateTable->str.len);
				stringpool.free = nextptr;
			} else
			{
				xlateTable = (mval *)mcalloc(SIZEOF(mval));
				xlateTable->str.addr = NULL;
				xlateTable->mvtype = MV_STR;
				xlateTable->str.len = 0;
			}
			args[3]->operand[0] = put_lit(xlateTable);
			xlateTable = (mval *)mcalloc(SIZEOF(mval));
			xlateTable->str.addr = (char *)xlate;
			xlateTable->mvtype = MV_STR;
			xlateTable->str.len = NUM_CHARS * SIZEOF(int4);
			args[2]->operand[0] = put_lit(xlateTable);
			/* Setup triple structure */
			args[0]->operand[1] = put_tref(args[1]);
			args[1]->operand[1] = put_tref(args[2]);
			args[2]->operand[1] = put_tref(args[3]);
		}
	}
	ins_triple(args[0]);
	*a = put_tref(args[0]);
	return TRUE;
}
