/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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

#include <stdarg.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "fnname.h"
#include "op.h"
#include "gvsub2str.h"
#include "mvalconv.h"
#include "get_reference.h"	/* for get_reference() prototype */

GBLREF gv_key		*gv_currkey;
GBLREF mstr		extnam_str;
GBLREF mv_stent		*mv_chain;
GBLREF spdesc		stringpool;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;

error_def(ERR_MAXSTRLEN);
error_def(ERR_GVNAKED);
error_def(ERR_FNNAMENEG);
error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

#ifdef  TEST_DOLLAR_NAME_GCOL
#define TEST_FAKE_STRINGPOOL_FULL	stringpool.free = stringpool.top /* force gcol at various stages of $NAME processing */
#else
#define TEST_FAKE_STRINGPOOL_FULL
#endif

#define COPY_ARG_TO_STP(ARG, SPACE_NEEDED, FORMAT_OUT, DST)							\
{														\
	TEST_FAKE_STRINGPOOL_FULL;										\
	assert(IS_AT_END_OF_STRINGPOOL(DST->str.addr, DST->str.len));						\
	mval_lex(ARG, &FORMAT_OUT);										\
	if (IS_AT_END_OF_STRINGPOOL(DST->str.addr, DST->str.len))						\
	{	/* DST is already at stringpool end. All that is needed is to copy FORMAT_OUT after it. */	\
		memcpy(stringpool.free, FORMAT_OUT.addr, FORMAT_OUT.len);					\
		stringpool.free += FORMAT_OUT.len;								\
		DST->str.len += FORMAT_OUT.len;									\
	} else if (IS_AT_END_OF_STRINGPOOL(FORMAT_OUT.addr, FORMAT_OUT.len)					\
			&& ((DST->str.addr + DST->str.len) == FORMAT_OUT.addr))					\
	{	/* DST is no longer at stringpool end but it is immediately followed by FORMAT_OUT and		\
		 * together they are in sequence leading to stringpool end. Even less to do.			\
		 */												\
		DST->str.len += FORMAT_OUT.len;									\
	} else													\
	{	/* "DST" and FORMAT_OUT point to strings that are not in stringpool end. This is possible	\
		 * if "mval_lex()" invoked "stp_gcol()" and that in turn invoked "stp_gcol_nosort()".		\
		 * Need to move "DST" and "FORMAT_OUT" back to the end of the stringpool. We expect the		\
		 * caller to have already protected DST from garbage collection. But FORMAT_OUT would not	\
		 * so protect it by doing the PUSH_MV_STENT below.						\
		 */												\
		mval    *v2;											\
														\
		PUSH_MV_STENT(MVST_MVAL);									\
		v2 = &mv_chain->mv_st_cont.mvs_mval;								\
		v2->mvtype = MV_STR;										\
		v2->str = FORMAT_OUT;										\
		if (FORMAT_OUT.addr == (char *)stringpool.free)							\
			stringpool.free += FORMAT_OUT.len;							\
		ENSURE_IS_AT_END_OF_STRINGPOOL(DST->str.addr, DST->str.len, FORMAT_OUT.len + SPACE_NEEDED);	\
		stringpool.free += DST->str.len;								\
		FORMAT_OUT = v2->str;										\
		POP_MV_STENT();         /* v2 */								\
		memcpy(stringpool.free, FORMAT_OUT.addr, FORMAT_OUT.len);					\
		stringpool.free += FORMAT_OUT.len;								\
		DST->str.len += FORMAT_OUT.len;									\
	}													\
}														\

#define COPY_SUBSCRIPTS(SPACE_NEEDED, FORMAT_OUT, DST)					\
	for ( ; ; ) 									\
	{										\
		arg = va_arg(var, mval *);						\
		COPY_ARG_TO_STP(arg, SPACE_NEEDED, FORMAT_OUT, DST);			\
		depth_count--;								\
		if (0 == depth_count)							\
			break;								\
		*stringpool.free++ = ',';						\
		DST->str.len++;								\
	}

/* Implementation note: $NAME does not edit check the result, such as if the key size exceeds the maximum for a global.
 * So, the result if used in other operations (such as SET, KILL) may generate run time errors (GVSUBOFLOW, etc)
 */
void op_fnname(UNIX_ONLY_COMMA(int sub_count) mval *finaldst, ...)
{
	int 		depth_count, fnname_type, len, space_needed;;
	mval		*arg, *depth, *dst;
	mstr		format_out, opstr;
	va_list		var;
	unsigned char	*key_ptr, *key_top, *sptr;

	VAR_START(var, finaldst);
	assert(3 <= sub_count);
	fnname_type = va_arg(var, int);
	depth = va_arg(var, mval *); /* if second arg to $NAME not specified, compiler sets depth_count to MAXPOSINT4 */
	depth_count = MV_FORCE_INT(depth);
	sub_count -= 3;
	if (depth_count < 0)
	{
		va_end(var);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_FNNAMENEG);
	}
	/* Note about garbage collection : *dst and all possible *arg's in this function are anchored in the stringpool chains
	 * and preserved during garbage collection (run time argument mvals provided by compiler). So, if we maintain dst->str.len
	 * as we copy stuff to the stringpool, we are protected from mval_lex->stp_gcol shuffling strings around. Since the
	 * temporary allocation algorithm used by the compiler may re-use temporaries, it is possible that one of the subscript
	 * arguments is the destination for the result of $NAME. We have to assign the result mval as the last step, only after
	 * all arguments have been processed. We create a temporary place-holder for the result on the M stack so that the result
	 * is also on the stringpool chain and preserved during garbage collection.
	 * e.g. S X(1)=$NAME($J("hello",100))
	 * The result of $J is a temporary that is also an argument to $NAME. This temporary also happens to be the temporary to
	 * hold the result of $NAME.
	 */
	PUSH_MV_STENT(MVST_MVAL); /* create a temporary on M stack */
	dst = &mv_chain->mv_st_cont.mvs_mval;
	dst->mvtype = MV_STR;
	dst->str.len = 0;
	if (fnname_type == FNNAKGBL)
	{
		if (IS_ILLEGAL_GVNAKED(gv_currkey))
		{
			va_end(var);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_GVNAKED);
		}
		/* Reserve enough space for naked reference. Include space for ^() and a maximum of sub_count ',' separators for
		 * subscripts specified as arguments to $NAME() in addition to those in the naked reference
		 */
		space_needed = MAX_ZWR_KEY_SZ + (!extnam_str.len ? 0 : ((extnam_str.len * 2) + SIZEOF("^|\"\"|")));
		TEST_FAKE_STRINGPOOL_FULL;
		ENSURE_STP_FREE_SPACE(space_needed);
		sptr = stringpool.free;
		get_reference(dst);							/*this handles any extended reference */
		assert(dst->str.len);							/* should have caused GVNAKED error above */
		assert(dst->str.addr == (char *)sptr);
		for (len = dst->str.len; ('(' != *sptr++) && len--;)			/* step past the name returned */
			;
		assert(0 < len);							/* otherwise GVNAKED error above */
		if (0 != depth_count)
		{	/* the return from get_reference already has text, but this needs to count subscripts */
			key_top = (unsigned char *)&gv_currkey->base[gv_currkey->prev];
			for (key_ptr = (unsigned char *)&gv_currkey->base[0]; *key_ptr; key_ptr++)	/* step past name in key */
				;
			if (++key_ptr < key_top)
			{
				do
				{
					opstr.addr = (char *)sptr;
					opstr.len = space_needed;
					sptr = gvsub2str(key_ptr, &opstr, TRUE); /* gvsub2str assumes enough buffer available */
					while (*key_ptr++)
						;
					assert(0 <= depth_count);
					if (depth_count != MAXPOSINT4) /* although this may not make a difference in reality, */
						depth_count--;	       /* do not disturb depth_count (used later) if default  */
					*sptr++ = ',';
					if (0 == depth_count       /* fewer subscripts requested than in naked reference */
					    || key_ptr >= key_top) /* more subscripts requested than in naked reference */
						break;
				} while (depth_count);
			}
			/* Naked reference copied, now copy remaining subscripts. From this point on, maintain dst to protect
			 * against potential string shuffling by mval_lex->stp_gcol
			 */
			dst->str.len = INTCAST((char *)sptr - dst->str.addr);
			assert(dst->str.len < space_needed);
			stringpool.free = sptr;
			depth_count = ((sub_count < depth_count) ? sub_count : depth_count);
			if (0 != depth_count)
			{
				COPY_SUBSCRIPTS(space_needed, format_out, dst);
			} else
			{ /* take off extra , if depth doesn't go into new subs */
				stringpool.free--;
				dst->str.len--;
			}
			*stringpool.free++ = ')';
			dst->str.len++;
			assert((unsigned char *)(dst->str.addr + dst->str.len) == stringpool.free);
		} else
		{ /* naked reference, zero depth => result is just the global name */
			dst->str.len = INTCAST((char *)sptr - dst->str.addr - 1);
			assert(dst->str.len < space_needed);
			stringpool.free = sptr;
		}
	} else
	{
		space_needed = (int)(STR_LIT_LEN("^[,]()") + MAX_MIDENT_LEN + sub_count - 1); 	/* ^[,]GLVN(max of sub_count-1 *
											 * subscript separator commas) */
		TEST_FAKE_STRINGPOOL_FULL;
		/* We don't account for subscripts here as they are processed by mval_lex which reserves space if necessary */
		ENSURE_STP_FREE_SPACE(space_needed);
		dst->str.addr = (char *)stringpool.free;
		if (fnname_type & FNGBL)
		{
			*stringpool.free++ = '^';
			dst->str.len++;
		}
		if (fnname_type & (FNEXTGBL1 | FNEXTGBL2))
		{
			*stringpool.free++ = ((fnname_type & FNVBAR) ? '|' : '[');
			dst->str.len++;
			arg = va_arg(var, mval *);
			COPY_ARG_TO_STP(arg, space_needed, format_out, dst);
			sub_count--;
			if (fnname_type & FNEXTGBL2)
			{
				*stringpool.free++ = ',';
				dst->str.len++;
				arg = va_arg(var, mval *);
				COPY_ARG_TO_STP(arg, space_needed, format_out, dst);
				sub_count--;
			}
			*stringpool.free++ = ((fnname_type & FNVBAR) ? '|' : ']');
			dst->str.len++;
		}
		arg = va_arg(var, mval *);
		assert(MV_IS_STRING(arg) && (arg->str.len <= MAX_MIDENT_LEN));
		memcpy(stringpool.free, arg->str.addr, arg->str.len);
		stringpool.free += arg->str.len;
		dst->str.len += arg->str.len;
		sub_count--;
		depth_count = ((sub_count < depth_count) ? sub_count : depth_count);
		if (0 != depth_count)
		{
			*stringpool.free++ = '(';
			dst->str.len++;
			COPY_SUBSCRIPTS(space_needed, format_out, dst);
			*stringpool.free++ = ')';
			dst->str.len++;
		}
	}
	va_end(var);
	if (MAX_STRLEN < dst->str.len)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
	*finaldst = *dst;
	POP_MV_STENT(); /* don't need no temporary no more */
	return;
}
