/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
#include "rtnhdr.h"
#include "mv_stent.h"
#include "fnname.h"
#include "op.h"
#include "gvsub2str.h"
#include "mvalconv.h"

GBLREF gv_key		*gv_currkey;
GBLREF spdesc		stringpool;
GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackwarn, *stacktop;

#ifdef  TEST_DOLLAR_NAME_GCOL
#define TEST_FAKE_STRINGPOOL_FULL	stringpool.free = stringpool.top /* force gcol at various stages of $NAME processing */
#else
#define TEST_FAKE_STRINGPOOL_FULL
#endif

#define COPY_ARG_TO_STP									\
	{										\
		has_str_repsn = MV_IS_STRING(arg);					\
		TEST_FAKE_STRINGPOOL_FULL;						\
		mval_lex(arg, &format_out);						\
		if (format_out.addr != (char *)stringpool.free && has_str_repsn)	\
		{ /*  mval_lex doesn't create string representation for canonical arg that already has a string representation. */ \
			assert(arg->str.len  == format_out.len );			\
			assert(arg->str.addr == format_out.addr);			\
			TEST_FAKE_STRINGPOOL_FULL;					\
			if (stringpool.top - stringpool.free < arg->str.len)		\
				stp_gcol(arg->str.len);					\
			memcpy(stringpool.free, arg->str.addr, arg->str.len); /* use arg 'coz gcol doesn't preserve format_out */ \
		}									\
		if (has_str_repsn)							\
		{ /* mval_lex copies arg at stringpool.free and leaves stringpool.free unchanged. Caller has to change  */ \
		  /* stringpool.free to keep dst protected. MV_FORCE_STR in mval_lex creates string representation at   */ \
		  /* format.out for canonical numbers that did not have string representation updating stringpool.free. */ \
		  /* No need to update stringpool.free for such cases.							*/ \
			stringpool.free += format_out.len;				\
		}									\
	}										\

#define COPY_SUBSCRIPTS 								\
	for ( ; ; ) 									\
	{										\
		arg = va_arg(var, mval *);						\
		COPY_ARG_TO_STP;							\
		dst->str.len += format_out.len;						\
		depth_count--;								\
		if (0 == depth_count)							\
			break;								\
		*stringpool.free++ = ',';						\
		dst->str.len++;								\
	}

void op_fnname(UNIX_ONLY_COMMA(int sub_count) mval *finaldst, ...)
{
	int		space_needed;
	int 		depth_count, fnname_type;
	mval		*dst, *arg, *depth;
	VMS_ONLY(int	sub_count;)
	mstr		format_out;
	va_list		var;
	unsigned char	*sptr, *key_ptr, *key_top;
	boolean_t	has_str_repsn;
	error_def(ERR_MAXSTRLEN);
	error_def(ERR_GVNAKED);
	error_def(ERR_FNNAMENEG);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);

	/* Implementation note: $NAME does not edit check the result, such as if the key size exceeds the maximum for a global.
	 * So, the result if used in other operations (such as SET, KILL) may generate run time errors (GVSUBOFLOW, etc) */

	VAR_START(var, finaldst);
	VMS_ONLY(va_count(sub_count);)
	assert(3 <= sub_count);
	fnname_type = va_arg(var, int);
	depth = va_arg(var, mval *); /* if second arg to $NAME not specified, compiler sets depth_count to MAXPOSINT4 */
	depth_count = MV_FORCE_INT(depth);
	sub_count -=3;

	if (depth_count < 0)
	{
		va_end(var);
		rts_error(VARLSTCNT(1) ERR_FNNAMENEG);
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
		if (!gv_currkey || gv_currkey->prev == 0)
		{
			va_end(var);
			rts_error(VARLSTCNT(1) ERR_GVNAKED);
		}
		/* Reserve enough space for naked reference. Include space for ^() and a maximum of sub_count ',' separators for
		 * subscripts specified as arguments to $NAME() in addition to those in the naked reference */
		space_needed = (STR_LIT_LEN("^()") + MAX_ZWR_INFLATION * MAX_KEY_SZ + sub_count);
		TEST_FAKE_STRINGPOOL_FULL;
		if (space_needed > stringpool.top - stringpool.free)
			stp_gcol(space_needed);
		sptr = stringpool.free;
		*sptr++ = '^';
		key_ptr = (unsigned char *)&gv_currkey->base[0];
		key_top = (unsigned char *)&gv_currkey->base[gv_currkey->prev];
		for ( ; (*sptr = *key_ptr++); sptr++)
			;
		if (0 != depth_count)
		{
			*sptr++ = '(';
			if (key_ptr < key_top)
			{
				for ( ; ; )
				{
					sptr = gvsub2str(key_ptr, sptr, TRUE); /* gvsub2str assumes enough buffer available */
					while (*key_ptr++)
						;
					if (depth_count != MAXPOSINT4) /* although this may not make a difference in reality, */
						depth_count--;	       /* do not disturb depth_count (used later) if default  */
					*sptr++ = ',';
					if (0 == depth_count       /* fewer subscripts requested than in naked reference */
					    || key_ptr >= key_top) /* more subscripts requested than in naked reference */
						break;
				}
			}
			/* Naked reference copied, now copy remaining subscripts. From this point on, maintain dst to protect
			 * against potential string shuffling by mval_lex->stp_gcol
			 */
			dst->str.len = sptr - stringpool.free;
			dst->str.addr = (char *)stringpool.free;
			assert(dst->str.len < space_needed);
			stringpool.free = sptr;
			depth_count = ((sub_count < depth_count) ? sub_count : depth_count);
			if (0 != depth_count)
			{
				COPY_SUBSCRIPTS;
			} else
			{ /* take off extra , if depth doesn't go into new subs */
				stringpool.free--;
				dst->str.len--;
			}
			*stringpool.free++ = ')';
			dst->str.len++;
		} else
		{ /* naked reference, zero depth => result is just the global name */
			dst->str.len = sptr - stringpool.free;
			dst->str.addr = (char *)stringpool.free;
			assert(dst->str.len < space_needed);
			stringpool.free = sptr;
		}
	} else
	{
		space_needed = STR_LIT_LEN("^[,]()") + MAX_MIDENT_LEN + sub_count - 1; 	/* ^[,]GLVN(max of sub_count-1 *
											 * subscript separator commas) */
		TEST_FAKE_STRINGPOOL_FULL;
		if (space_needed > stringpool.top - stringpool.free) /* We don't account for subscripts here as they are        */
			 stp_gcol(space_needed);		     /* processed by mval_lex which reserves space if necessary */
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
			COPY_ARG_TO_STP;
			dst->str.len += format_out.len;
			sub_count--;
			if (fnname_type & FNEXTGBL2)
			{
				*stringpool.free++ = ',';
				dst->str.len++;
				arg = va_arg(var, mval *);
				COPY_ARG_TO_STP;
				dst->str.len += format_out.len;
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
			COPY_SUBSCRIPTS;
			*stringpool.free++ = ')';
			dst->str.len++;
		}
	}
	va_end(var);
	if (MAX_STRLEN < dst->str.len)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	*finaldst = *dst;
	POP_MV_STENT(); /* don't need no temporary no more */
	return;
}
