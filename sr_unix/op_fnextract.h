/****************************************************************
 *								*
 *	Copyright 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note this routine is built as op_fnextract() on all but IA64 platforms
   where it is instead built as op_fnextract2() due to linkage requirements
   where values in the transfer table must be consistently assembler or
   "C" for the correct call signature to be made. Since this routine is
   changed in the transfer table under certain conditions (unicode or not),
   the interface needed to be consistent so an assembler stub is made to
   call this C routine to match the alternate op_fnzextract that could be
   in that transfer table slot.
*/

GBLREF	boolean_t	badchar_inhibit;

void OP_FNEXTRACT(int last, int first, mval *src, mval *dest)
{
	char	*srcbase, *srctop, *srcptr;
	int	len, skip, bytelen;

	MV_FORCE_STR(src);
	MV_INIT(dest);
	dest->mvtype = MV_STR;

	if (first <= 0)
		first = 1;
	else if (first > src->str.len)
	{
		dest->str.len = 0;
		return;
	}
	if (last > 0 && last > src->str.len)
		last = src->str.len;

	if (MV_IS_SINGLEBYTE(src))
	{	/* fast-path extraction of an entirely single byte string */
		if ((len = last - first + 1) > 0)
		{
			dest->str.addr = src->str.addr + first - 1;
			dest->str.len = len;
			if (badchar_inhibit)
			{
				dest->str.char_len = dest->str.len;
				dest->mvtype |= MV_UTF_LEN;
			} else
				MV_FORCE_LEN(dest); /* catch BADCHARs (if any) */
		} else
			dest->str.len = 0;
	} else
	{	/* generic extraction of a multi-byte string */
		if ((len = last - first + 1) <= 0)
		{
			dest->str.len = 0;
			return;
		}
		srcbase = src->str.addr;
		srctop = srcbase + src->str.len;
		for (srcptr = srcbase, skip = first - 1; (skip > 0 && srcptr < srctop); --skip)
		{ /* skip to the character position 'first' */
			if (!UTF8_VALID(srcptr, srctop, bytelen) && !badchar_inhibit)
				UTF8_BADCHAR(0, srcptr, srctop, 0, NULL);
			srcptr += bytelen;
		}
		assert(srcptr <= srctop);
		if (skip > 0)
		{ /* first position is past the last character */
			dest->str.len = 0;
			return;
		}
		dest->str.addr = srcbase = srcptr;
		if (srcbase + len >= srctop)
		{	/* A more efficient implementation of usages like $E(str,99999) where there is no need */
			/* to scan the rest of the string unless BADCHAR errors need to be caught */
			dest->str.len = INTCAST(srctop - srcbase);
			if (!badchar_inhibit)
				MV_FORCE_LEN(dest);
		} else
		{	/* Skip the next 'len' characters and trigger BADCHAR if need to be caught */
			for (skip = len; (skip > 0 && srcptr < srctop); --skip)
			{
				if (!UTF8_VALID(srcptr, srctop, bytelen) && !badchar_inhibit)
					UTF8_BADCHAR(0, srcptr, srctop, 0, NULL);
				srcptr += bytelen;
			}
			assert(srcptr <= srctop);
			dest->str.len = INTCAST(srcptr - srcbase);
			dest->str.char_len = len - skip;
			dest->mvtype |= MV_UTF_LEN;
		}
	}
}
