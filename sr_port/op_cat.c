/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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

#define MAX_NUM_LEN 64

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
	mval 			*in, *src;
	va_list			var;
	VMS_ONLY(int srcargs;)

	VAR_START(var, dst);
	VMS_ONLY(va_count(srcargs);)
	srcargs -= 1;			/* account for dst */
	/* Section 1: Determine if garbage collection is required by estimating the space needed to concat.
	 * This is inexact for two reasons: 1) we can't know the string length of a num until conversion-time,
	 * which must be deferred to concat-time since conversion adds to the stringpool, and 2) we can't
	 * account for reuse of adjoining source values without depending on a stable sort within stp_gcol
	 * (imagine if we need space for the final two of four source vals, then garbage collection reordered the
	 * stringpool such that the first two were no longer in order at the end of the stringpool). Future
	 * maintainers should consider that while assuming stable sort would increase coupling it would also
	 * make it possible to combine sections 1 and 2 and get a more accurate estimate of space needed. */
	maxlen = 0;
	for (i = 0; i < srcargs ; i++)
	{
		in = va_arg(var, mval *);
		maxlen += MV_IS_STRING(in) ? in->str.len : MAX_NUM_LEN;
		if (maxlen > MAX_STRLEN)
		{
			va_end(var);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
		}
	}
	va_end(var);
	ENSURE_STP_FREE_SPACE(maxlen);
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
	 * case 3 with the well-situated src values set aside.*/
	VAR_START(var, dst);
	in = va_arg(var, mval *);
	if (MV_IS_STRING(in)
	    && ((unsigned char *)in->str.addr >= stringpool.base)
	    && ((unsigned char *)in->str.addr + in->str.len <= stringpool.free))
	{
		base = (unsigned char *)in->str.addr;
		cp = base + in->str.len;
		for (i = 1; i < srcargs; i++)
		{
			in = va_arg(var, mval *);
			if (MV_IS_STRING(in) && (unsigned char *)in->str.addr == cp)
				cp += in->str.len;
			else if (cp == stringpool.free) /*BYPASSOK*/
				break;
			else
			{
				i = 0;
				base = cp = stringpool.free;
				va_end(var);
				VAR_START(var, dst);
				in = va_arg(var, mval *);
				break;
			}
		}
	} else
	{
		i = 0;
		base = cp = stringpool.free;
	}
	/* Section 3: Perform necessary memcpy/n2s operations. The logic above guarantees that base will point to
	 * the address of the string in construction, cp will point to stringpool.free (if any ops are needed),
	 * and that srcargs - i will equal the number of memcpy/n2s operations to perform */
	for (; i < srcargs; (++i >= srcargs) || (in = va_arg(var, mval *)))
	{
		assert(cp == stringpool.free);
		MV_FORCE_DEFINED(in);
		/* Now that we have ensured enough space in the stringpool, we dont expect any more
		 * garbage collections or expansions until we are done with the concatenation. We need
		 * to do this AFTER the MV_FORCE_DEFINED since that can trigger an error and in that case
		 * we dont want this debug global variable set for post-error-trap M code.
		 */
		DBG_MARK_STRINGPOOL_UNEXPANDABLE;
		if (MV_IS_STRING(in))
			memcpy(cp, in->str.addr, in->str.len);
		else
		{
			/* Convert to string, rely on the fact that it will be converted
			 * exactly at the end of the stringpool. */
			n2s(in);
			assert(stringpool.free == (unsigned char *)in->str.addr + in->str.len);
		}
		cp += in->str.len;
		assert(cp >= stringpool.free);
		/* The following assignment is safe since we know that cp is always > stringpool.free
		 * in this section. This will need to change if that assumpion is ever violated */
		stringpool.free = cp;
		/* Now that we are done with stringpool.free initializations for this iteration,
		 * mark as free for expansion */
		DBG_MARK_STRINGPOOL_EXPANDABLE;
	}
	va_end(var);
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)base;
	dst->str.len = INTCAST(cp - base);
	return;
}
