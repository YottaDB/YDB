/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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

GBLREF	boolean_t	badchar_inhibit, gtm_utf8_mode;

LITREF mval literal_null;

#define	MAX_TRANSLATE_ARGS	4

int f_translate(oprtype *a, opctype op)
{
	boolean_t	more_args;
	hash_table_int4 *xlate_hash;
	int4		i, maxLengthString;
	mval		dst_mval, *rplc_mval, *srch_mval, *xlateTable[2];
	sm_uc_ptr_t	nextptr;
	triple		*args[MAX_TRANSLATE_ARGS + 1];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	args[0] = maketriple(op);
	if (EXPR_FAIL == expr(&(args[0]->operand[0]), MUMPS_EXPR))
		return FALSE;
	for (i = 1, more_args = TRUE; i < MAX_TRANSLATE_ARGS; i++)
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
	/* If the second, third, and fourth args are literals, pre-calculate the translation table and store it in the stringpool */
	if ((OC_LIT == args[1]->operand[0].oprval.tref->opcode) && (OC_LIT == args[2]->operand[0].oprval.tref->opcode) &&
			(OC_LIT == args[3]->operand[0].oprval.tref->opcode))
	{	/* we only do this if we have search and reolace literals */
		srch_mval = &args[1]->operand[0].oprval.tref->operand[0].oprval.mlit->v;
		rplc_mval = &args[2]->operand[0].oprval.tref->operand[0].oprval.mlit->v;
		assert(MV_STR & srch_mval->mvtype);
		assert(MV_STR & rplc_mval->mvtype);
		xlateTable[0] = (mval *)mcalloc(SIZEOF(mval));
		xlateTable[0]->mvtype = 0;	/* so stp_gcol, which may be invoked below by ENSURE..., does not get confused */
		maxLengthString = (NUM_CHARS * SIZEOF(int4));
		if (!gtm_utf8_mode || (OC_FNZTRANSLATE == op))
		{	/* just doing bytes - no need for a hash table */
			if (OC_LIT == args[0]->operand[0].oprval.tref->opcode)	/* if lit src, dst can't be longer */
				maxLengthString += args[0]->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len;
			ENSURE_STP_FREE_SPACE(maxLengthString);
			xlateTable[0]->str.addr = (char *)stringpool.free;
			xlateTable[0]->mvtype = MV_STR;
			xlateTable[0]->str.len = NUM_CHARS * SIZEOF(int4);
			stringpool.free += NUM_CHARS * SIZEOF(int4);
			create_byte_xlate_table(srch_mval, rplc_mval, (int *)xlateTable[0]->str.addr);
			if (OC_LIT == args[0]->operand[0].oprval.tref->opcode)
			{	/* the source is a literal too - lets do it all at compile time */
				op_fnztranslate_fast(&args[0]->operand[0].oprval.tref->operand[0].oprval.mlit->v,
					xlateTable[0], &args[3]->operand[0].oprval.tref->operand[0].oprval.mlit->v, &dst_mval);
				unuse_literal(&args[0]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				dqdel(args[0]->operand[0].oprval.tref, exorder);
				args[0]->opcode = OC_LIT;
				put_lit_s(&dst_mval, args[0]);
				args[0]->operand[1].oprclass = NO_REF;
				unuse_literal(&args[1]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				unuse_literal(&args[2]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				unuse_literal(&args[3]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				dqdel(args[1]->operand[0].oprval.tref, exorder);
				dqdel(args[2]->operand[0].oprval.tref, exorder);
				dqdel(args[3]->operand[0].oprval.tref, exorder);
				dqdel(args[1], exorder);
				dqdel(args[2], exorder);
				dqdel(args[3], exorder);
			} else
			{
				args[0]->opcode = OC_FNZTRANSLATE_FAST;
				args[1]->operand[0] = put_lit(xlateTable[0]);
				args[2]->operand[0] = args[3]->operand[0];
				/* bind up triple structure */
				args[0]->operand[1] = put_tref(args[1]);
				args[1]->operand[1] = put_tref(args[2]);
			}
		} else if (gtm_utf8_mode && valid_utf_string(&srch_mval->str) && valid_utf_string(&rplc_mval->str))
		{	/* actual UTF-8 characters, so need hashtable rather than just than code table */
			if (!badchar_inhibit)
				MV_FORCE_LEN(srch_mval);      				/* needed only to validate for BADCHARs */
			else
				MV_FORCE_LEN_SILENT(srch_mval);				/* but need some sorta valid length */
			maxLengthString = xlateTable[0]->str.len = MAX(srch_mval->str.char_len, maxLengthString);
			if (OC_LIT == args[0]->operand[0].oprval.tref->opcode)		/* if lit src, dst can't be longer */
				maxLengthString						/* because compile puts hash in stp */
					+= (args[0]->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len * MAX_CHAR_LEN);
			ENSURE_STP_FREE_SPACE(maxLengthString);
			xlateTable[0]->str.addr = (char *)stringpool.free;
			xlateTable[0]->mvtype = MV_STR;
			stringpool.free += xlateTable[0]->str.len;
			xlate_hash = create_utf8_xlate_table(srch_mval, rplc_mval, &xlateTable[0]->str);
			if (NULL != xlate_hash)
			{
				nextptr = copy_hashtab_to_buffer_int4(xlate_hash, stringpool.free, NULL);
				xlateTable[1] = (mval *)mcalloc(SIZEOF(mval));
				xlateTable[1]->str.addr = (char *)stringpool.free;
				xlateTable[1]->mvtype = MV_STR;
				xlateTable[1]->str.len = nextptr - stringpool.free;
				stringpool.free = nextptr;
			} else
				xlateTable[1] = (mval *)&literal_null;
			if ((OC_LIT == args[0]->operand[0].oprval.tref->opcode)
				&& valid_utf_string(&args[0]->operand[0].oprval.tref->operand[0].oprval.mlit->v.str))
			{	/* the source is a literal too - lets do it all at compile time */
				op_fntranslate_fast(&args[0]->operand[0].oprval.tref->operand[0].oprval.mlit->v, rplc_mval,
						xlateTable[0], &args[3]->operand[0].oprval.tref->operand[0].oprval.mlit->v,
							xlateTable[1], &dst_mval);
				unuse_literal(&args[0]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				dqdel(args[0]->operand[0].oprval.tref, exorder);
				args[0]->opcode = OC_LIT;
				put_lit_s(&dst_mval, args[0]);
				args[0]->operand[1].oprclass = NO_REF;
				unuse_literal(&args[1]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				unuse_literal(&args[2]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				unuse_literal(&args[3]->operand[0].oprval.tref->operand[0].oprval.mlit->v);
				dqdel(args[1]->operand[0].oprval.tref, exorder);
				dqdel(args[2]->operand[0].oprval.tref, exorder);
				dqdel(args[3]->operand[0].oprval.tref, exorder);
				dqdel(args[1], exorder);
				dqdel(args[2], exorder);
				dqdel(args[3], exorder);
			} else
			{	/* op_fntranslate_fast arguments; src, rplc, m_xlate, dir, xlate_hash, so need one more triple */
				args[0]->opcode = OC_FNTRANSLATE_FAST;		/* note no Z */
				assert(OC_PARAMETER == args[1]->opcode);
				args[1]->operand[0] = args[2]->operand[0];	/* Promote the rplc string to the second argument */
				assert(OC_PARAMETER == args[2]->opcode);
				args[2]->operand[0] = put_lit(xlateTable[0]);
				/* args[3]->operand[0] = args[3]->operand[0]; - NO OP */
				args[4] = newtriple(OC_PARAMETER);
				args[4]->operand[0] = put_lit(xlateTable[1]);
				/* bind up triple structure */
				args[0]->operand[1] = put_tref(args[1]);
				args[1]->operand[1] = put_tref(args[2]);
				args[2]->operand[1] = put_tref(args[3]);
				args[3]->operand[1] = put_tref(args[4]);
			}
		}
	}
	ins_triple(args[0]);
	*a = put_tref(args[0]);
	return TRUE;
}
