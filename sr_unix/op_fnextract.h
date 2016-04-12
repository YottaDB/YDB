/****************************************************************
 *								*
 * Copyright (c) 2008-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note this routine is built as op_fnextract() on all but IA64 platforms
 * where it is instead built as op_fnextract2() due to linkage requirements
 * where values in the transfer table must be consistently assembler or
 * "C" for the correct call signature to be made. Since this routine is
 * changed in the transfer table under certain conditions (unicode or not),
 * the interface needed to be consistent so an assembler stub is made to
 * call this C routine to match the alternate op_fnzextract that could be
 * in that transfer table slot.
 *
 * Parameters:
 *
 *    last   - last character position to extract from source string.
 *    first  - first character position to extract from source string.
 *    src    - the actual source string.
 *    dest   - destination mval for extracted string.
 *
 * Note src and dest can be the same mval in certain situations like "SET X=$EXTRACT(X,2,5)"
 * so dest fields should only be modified once we no longer need the same src field.
 */

DBGUTFC_ONLY(STATICDEF uint4 xtrctcnt;)

GBLREF boolean_t	badchar_inhibit;
GBLREF boolean_t	gtm_utf8_mode;

void OP_FNEXTRACT(int last, int first, mval *src, mval *dest)
{
	char			*srcbase, *srctop, *srcptr;
	unsigned char		*cptr, *cstart, *ctop;
	int			len, skip, bytelen, clen, first_charcnt;
#	ifdef UNICODE_SUPPORTED
	utfscan_parseblk	utf_parse_blk;
	DEBUG_ONLY(utfscan_parseblk utf_parse_blk_save;)
	boolean_t		success, found_start, utf_parse_blk_setup;
#	endif
	DCL_THREADGBL_ACCESS;

	if (!gtm_utf8_mode)
	{	/* Might be called directly from compiler with no check for utf8 mode or not */
		op_fnzextract(last, first, src, dest);
		return;
	}
	SETUP_THREADGBL_ACCESS;
	assert(!TREF(compile_time) || valid_utf_string(&src->str));
	MV_FORCE_STR(src);
	if (0 >= first)
		first = 1;
	else if (first > src->str.len)
	{
		dest->mvtype = MV_STR;
		dest->str.len = 0;
		return;
	}
	if ((0 < last) && (last > src->str.len))
		last = (src->mvtype & MV_UTF_LEN) ? src->str.char_len : src->str.len;
	if (MV_IS_SINGLEBYTE(src))
	{	/* Fast-path extraction of an entirely single byte string (char_len == len) */
		DBGUTFC((stderr, "\nop_fnextract(%d): M mode extraction - mval: 0x"lvaddr"  mval->str.len: %d  "
			 "mval->str.addr: 0x"lvaddr"  first: %d  last: %d\n", ++xtrctcnt, src, src->str.len, src->str.addr,
			 first, last));
		if (0 < (len = last - first + 1))			/* Note assignment */
		{
			cstart = (unsigned char *)(src->str.addr + first - 1);
			if (!badchar_inhibit)
			{	/* We need to care about whether there are any BADCHARs in this string so run through them
				 * right quick and look for non-ASCII chars since we already know these are all single-byte
				 * characters (no multi-byte characters). Since we'll know char_len, set that too.
				 */
				cptr = cstart;
				ctop = cstart + len;
				for (clen = 0; cptr < ctop; ++clen, cptr++)
				{
					if (ASCII_MAX < *cptr)
						UTF8_BADCHAR(0, cptr, ctop, 0, NULL);
				}
				assert(clen == len);
				dest->str.char_len = clen;
				dest->mvtype = MV_STR | MV_UTF_LEN;
			} else
				dest->mvtype = MV_STR;
			dest->str.addr = (char *)cstart;
			dest->str.len = len;
		} else
		{
			dest->mvtype = MV_STR;
			dest->str.len = 0;
		}
		DBGUTFC((stderr, "op_fnextract(%d): Return value: length %d  string: %.*s\n", xtrctcnt, dest->str.len,
			 dest->str.len, dest->str.addr));
	} else
	{	/* Generic extraction of a multi-byte string or UTF8 mode and length unknown */
#		ifdef UNICODE_SUPPORTED
		utf_parse_blk_setup = FALSE;
		if (0 >= (last - first + 1))
		{	/* first is > last - return NULL string */
			dest->mvtype = MV_STR;
			dest->str.len = 0;
			return;
		}
		srcbase = src->str.addr;
		srctop = srcbase + src->str.len;
		DBGUTFC((stderr, "\nop_fnextract(%d): UTF mode extraction - mval: 0x"lvaddr"  mval->str.len: %d  "
			 "mval->str.addr: 0x"lvaddr"  first: %d  last: %d\n", ++xtrctcnt, src, src->str.len, src->str.addr,
			 first, last));
		/* Locate starting extract position */
		if (1 < first)
		{
			utf_parse_blk.mv = src;
			utf_parse_blk.stoponbadchar = !badchar_inhibit;
			utf_parse_blk.scan_byte_offset = 0;		/* Start at first char */
			utf_parse_blk_setup = TRUE;
			success = utfcgr_scanforcharN(first, &utf_parse_blk);
			DBGUTFC((stderr, "op_fnextract(%d): Return from utfcgr_scanforcharN(1st): success: %d"
				 "  utf_parse_blk.scan_byte_offset: %d  utf_parse_blk.scan_char_count: %d"
				 "  utf_parse_blk.scan_char_len: %d  utf_parse_blk.scan_char_type: %d\n",
				 xtrctcnt, success, utf_parse_blk.scan_byte_offset, utf_parse_blk.scan_char_count,
				 utf_parse_blk.scan_char_len, utf_parse_blk.scan_char_type));
			if (success)
			{	/* Scan succeeded - found starting place */
				found_start = TRUE;
				srcptr = src->str.addr + utf_parse_blk.scan_byte_offset;
			} else
			{	/* Scan failed - find out why */
				found_start = FALSE;			/* Didn't find starting char */
				if (UTFCGR_EOL == utf_parse_blk.scan_char_type)	/* If ran out of chars before finding Nth.. */
				{	/* Return 0 if character position exceeds chars available */
					dest->mvtype = MV_STR;
					dest->str.len = 0;
					return;
				} else if ((UTFCGR_BADCHAR == utf_parse_blk.scan_char_type) && !badchar_inhibit)
					/* Ran into a badchar that was not ignorable - no return */
					UTF8_BADCHAR(0, utf_parse_blk.badcharstr, utf_parse_blk.badchartop, 0, NULL);
				else
					assertpro(FALSE);		/* Unknown error - no return */
			}
			first_charcnt = utf_parse_blk.scan_char_count;		/* Save char count for "first" */
		} else
		{
			srcptr = srcbase;
			first_charcnt = 0;
		}
		assert(srcptr <= srctop);
		srcbase = srcptr;
		/* Now locate the last character of the extract */
		if (utf_parse_blk_setup)
		{
			DEBUG_ONLY(utf_parse_blk_save = utf_parse_blk);			/* Save first return for debugging */
		} else
		{	/* We didn't use utf_parse_blk to find first char so set it up now */
			utf_parse_blk.mv = src;
			utf_parse_blk.stoponbadchar = !badchar_inhibit;
			utf_parse_blk.scan_byte_offset = 0;			/* Start at first char */
		}
		success = utfcgr_scanforcharN(last, &utf_parse_blk);
		DBGUTFC((stderr, "op_fnextract(%d): Return from utfcgr_scanforcharN(end): success: %d"
			 "  utf_parse_blk.scan_byte_offset: %d  utf_parse_blk.scan_char_count: %d  utf_parse_blk.scan_char_len:"
			 " %d  utf_parse_blk.scan_char_type: %d\n", xtrctcnt, success, utf_parse_blk.scan_byte_offset,
			 utf_parse_blk.scan_char_count, utf_parse_blk.scan_char_len, utf_parse_blk.scan_char_type));
		if (!success)
		{	/* Scan failed - find out why */
			found_start = FALSE;
			if (UTFCGR_EOL == utf_parse_blk.scan_char_type)
			{	/* This is possibly due to $E(str,99999) sort of thing so we ran out of characters to scan.
				 * Pretend scan worked and let it pickup the scan termination values but in this case of EOL
				 * (end of line), we compute character length differently since the scan ended AFTER the
				 * last byte of the string instead of the char before the one we were looking for.
				 */
				dest->str.char_len = utf_parse_blk.scan_char_count - first_charcnt;
			} else if ((UTFCGR_BADCHAR == utf_parse_blk.scan_char_type) && !badchar_inhibit)
				/* Ran into a badchar that was not ignorable - no return */
				UTF8_BADCHAR(0, utf_parse_blk.badcharstr, utf_parse_blk.badchartop, 0, NULL);
			else
				assertpro(FALSE);			/* Unknown error - no return */
		} else
		{	/* Since utf_parse_blk.scan_char_count returns the length prior to the last character, we have to add 1
			 * to the length. Then part of the difference is also to add one so we add another one.
			 */
			dest->str.char_len = (utf_parse_blk.scan_char_count - first + 1) + 1;
		}
		srcptr = src->str.addr + utf_parse_blk.scan_byte_offset + utf_parse_blk.scan_char_len;
		assert(srcptr <= srctop);
		dest->mvtype = MV_STR;
		dest->str.addr = srcbase;
		dest->str.len = INTCAST(srcptr - srcbase);
		dest->mvtype |= MV_UTF_LEN;
		DBGUTFC((stderr, "op_fnextract(%d): Return value: length %d  string: %.*s\n", xtrctcnt, dest->str.len,
			 dest->str.len, dest->str.addr));
#		else
		assertpro(FALSE);					/* Shouldn't be here if not supported */
#		endif /* UNICODE_SUPPORTED */
	}
}
