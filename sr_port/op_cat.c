/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"
#include "op.h"
#include <stdarg.h>
#include "min_max.h"

GBLREF spdesc stringpool;

error_def(ERR_MAXSTRLEN);

/* Given an indefinite number of source mvals (literal, num, str, etc.) and a destination mval,
 * op_cat concatenates the source values in order and points the destination mval to the result.
 * op_cat DOES NOT guarantee that the result will be the last entry in the stringpool, only that it
 * will be entirely contained in [stringpool.base, stringpool.free - 1]. op_cat maintains all stringpool
 * invariants. At this time, op_cat does not depend on stringpool garbage collection maintaining the
 * order of strings inside the stringpool. */
void op_cat(UNIX_ONLY_COMMA(int srcargs) mval *dst, ...)
{
	unsigned char 		*cp, *base;
	int 			i, maxlen;
	mval 			**in;
	va_list			var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(3 <= srcargs);						/* a destination and at least two operands */
	VAR_START(var, dst);
	srcargs--;							/* account for dst */
	if (srcargs > TREF(cat_array_size))
	{
		if (TREF(cat_array_size))
		{
			assert (NULL != TREF(cat_array_base));
			free(TREF(cat_array_base));
		}
		TREF(cat_array_size) = MAX(64, srcargs);
		TREF(cat_array_base) = (mval **)malloc(sizeof(mval *) * TREF(cat_array_size));	/* gtm_malloc does intrrpts, errs */
	}
	in = (mval **)TREF(cat_array_base);
	/* Section 1: Determine if garbage collection is required by estimating the space needed to concat.
	 * This is inexact because we can't account for reuse of adjoining source values without depending on a
	 * stable sort within stp_gcol (imagine if we need space for the final two of four source vals, then garbage
	 * collection reordered the stringpool such that the first two were no longer in order at the end of the
	 * stringpool). Future maintainers should consider that while assuming stable sort would increase coupling
	 * it might also make it possible to combine sections 1 and 2 and get a more accurate estimate of space needed.
	 */
	in[0] = va_arg(var, mval *);		/* before loop because C compiler doesn't get that loop executes at least once */
	for (i = maxlen = 0; i < srcargs; srcargs--)
	{	/* toss leading empty strings, which can easily show up here with NOUNDEF mode */
		MV_FORCE_STR(in[0]);
		if (in[0]->str.len || (2 == srcargs))
		{	/* got substance or are to last arg, which we expect to be rare, but, if so, it works to just proceed */
			maxlen = in[i++]->str.len;
			break;
		}
		in[0] = va_arg(var, mval *);
	}
	for (; i < srcargs ; i++)
	{	/* accumulate a destination length */
		in[i] = va_arg(var, mval *);
		MV_FORCE_STR(in[i]);					/* cat is a string operation, to complete we need string */
		maxlen += in[i]->str.len;
		if (maxlen > MAX_STRLEN)
		{
			va_end(var);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
		}
	}
	va_end(var);
	ENSURE_STP_FREE_SPACE(maxlen);
	DBG_MARK_STRINGPOOL_UNEXPANDABLE;
	/* Section 2: Determine into which of three categories our concatenation falls. Say we are asked to
	 * concatenate strings A, B, and C into destination D. Let A="aaaa", B="bbbb", and C="cccc". The stringpool
	 * may be in one of three states (let any "x" indicate an arbitrary character not included in A, B, or C):
	 * Stringpool case 1: "xxxxaaaabbbbccccxxxx"
	 * The concatenation already exists latent in the stringpool. Point D to the start of A.
	 * Stringpool case 2: "ccccxxxxxxxxaaaabbbb"
	 * A prefix of our concatenation is a suffix of the stringpool. Point D to A and copy C to end.
	 * Stringpool case 3: "ccccxxxxaaaaxxxxbbbb"
	 * All other cases - full in-order memcpy of source values.
	 * Note that these do not depict cases when src vals are not already in the stringpool (state 2 or 3).
	 * Finally, note that the sequence of memcpys required by cases 1 and 2 is equivalent to what's required by
	 * case 3 with the well-situated src values set aside.
	 */
	if (((unsigned char *)in[0]->str.addr >= stringpool.base)
		&& ((unsigned char *)in[0]->str.addr + in[0]->str.len <= stringpool.free))
	{	/* effective first argument is already in the stringpool */
		base = (unsigned char *)in[0]->str.addr;
		cp = base + in[0]->str.len;				/* move past 0th argument */
		for (i = 1; i < srcargs; i++)
		{
			if (((unsigned char *)in[i]->str.addr == cp) || (0 == in[i]->str.len))
			{	/* arguments match so far or has no length and doesn't contribute to the copies */
				cp += in[i]->str.len;
				assert((cp - base) <= maxlen);
			} else if (cp == stringpool.free)		/* BYPASSOK */
					break;				/* matchs all the way to up to free */
			else
			{	/* luck ran out: something else is behind prefix, so place all args starting at stringpool_free */
				i = 0;
				base = cp = stringpool.free;
				break;
			}
		}
	} else
	{	/* no chance for optimization */
		i = 0;
		base = cp = stringpool.free;
	}
	/* Section 3: Perform necessary memcpy operations. The logic above guarantees that base will point to
	 * the address of the string in construction, cp will point to stringpool.free (if any ops are needed),
	 * and that srcargs - i will be the maximum number of memcpy operations to perform
	 */
	for (; i < srcargs; (++i >= srcargs))
	{	/* do copies for all arguments not matched above */
		assert(cp == stringpool.free);
		/* Now that we have ensured enough space in the stringpool, we dont expect any more
		 * garbage collections or expansions until we are done with the concatenation. We need
		 * to do this AFTER the MV_FORCE_DEFINED since that can trigger an error and in that case
		 * we dont want this debug global variable set for post-error-trap M code.
		 */
		if (in[i]->str.len)					/* avoid any zero length copy */
			memcpy(cp, in[i]->str.addr, in[i]->str.len);
		cp += in[i]->str.len;
		assert(((cp - base) <= maxlen) && (cp >= stringpool.free));
		/* following assignment is safe because cp is never less than stringpool.free in this section */
		stringpool.free = cp;
	}
	DBG_MARK_STRINGPOOL_EXPANDABLE;					/* all in the stringpool - mark as free for expansion */
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)base;
	dst->str.len = INTCAST(cp - base);
	assert((maxlen >= dst->str.len) && (MAX_STRLEN >= dst->str.len));
	return;
}
