/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "matchc.h"
#include "mvalconv.h"
#include "op.h"
#include "io.h"
#include "gtmio.h"

/*
 * -----------------------------------------------
 * op_fn[z]find()
 *
 * MUMPS Find function (op_fnfind() is UTF8 flavor, op_fnzfind() is ASCII/byte flavor)
 *
 * Arguments:
 *	src	- Pointer to Source string mval
 *	del	- Pointer to delimiter mval
 *	first	- starting index
 *	dst	- destination buffer to save the piece in
 *
 * Return:
 *	first character position after the delimiter match
 * -----------------------------------------------
 */

#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#include "utfcgr.h"

GBLREF	boolean_t	badchar_inhibit;
GBLREF	boolean_t	gtm_utf8_mode;

DBGUTFC_ONLY(STATICDEF uint4 findcnt;)

int4 op_fnfind(mval *src, mval *del, mint first, mval *dst)
{
	boolean_t		found_start, success;
	char 			*match, *srcptr, *srctop;
	int 			bytelen, match_res, numpcs, srclen;
	mint 			result;
	utfscan_parseblk	utf_parse_blk;

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);
	DBGUTFC((stderr, "\nop_fnfind(%d): Called for string 0x"lvaddr" with first value: %d\n", ++findcnt, src->str.addr, first));
	if (0 < first)
		first--;
	else
		first = 0;
	if (0 == del->str.len)
		result = first + 1;
	else if (src->str.len <= first)
		result = 0;
	else
	{
		/* Check specifically for gtm_utf8_mode here since op_fnfind is called directly from op_fnpopulation (two
		 * argument $LENGTH() call).
		 */
		if (!gtm_utf8_mode || (MV_IS_SINGLEBYTE(src) && badchar_inhibit))
		{	/* If BADCHARs are to be checked, matchb() shouldn't be used even if the source is entirely single byte */
			DBGUTFC((stderr, "op_fnfind(%d): Using matchb() for scan\n", findcnt));
			numpcs = 1;
			match = (char *)matchb(del->str.len, (uchar_ptr_t)del->str.addr,
					       src->str.len - first, (uchar_ptr_t)src->str.addr + first, &match_res, &numpcs);
			result = match_res ? first + match_res : 0;
		} else
		{	/* Either the string contains multi-byte characters or BADCHAR check is required */
			DBGUTFC((stderr, "op_fnfind(%d): Using matchc() for scan\n", findcnt));
			srctop = src->str.addr + src->str.len;			/* Top + 1 of input string */
			result = 0;
			if (0 < first)
			{	/* Locate the character index where we should start our search */
				utf_parse_blk.mv = src;
				utf_parse_blk.stoponbadchar = !badchar_inhibit;
				utf_parse_blk.scan_byte_offset = 0;		/* Start at first char */
				/* Locate starting character */
				success = utfcgr_scanforcharN(first + 1, &utf_parse_blk); /* Cvt back to char indx from offset */
				if (success)
				{
					/* Scan succeeded - found starting place */
					found_start = TRUE;
					DBGUTFC((stderr, "op_fnfind(%d) utfcgr_scanforcharN() offset result: %d\n", findcnt,
						 utf_parse_blk.scan_byte_offset));
					srcptr = src->str.addr + utf_parse_blk.scan_byte_offset;
				} else
				{	/* Scan failed - find out why */
					found_start = FALSE;			/* Didn't find starting char */
					if (UTFCGR_EOL == utf_parse_blk.scan_char_type)	/* Ran out of chars before finding Nth? */
						;				/* Fall through to return not found */
					else if ((UTFCGR_BADCHAR == utf_parse_blk.scan_char_type) && !badchar_inhibit)
						/* Ran into a badchar that was not ignorable - no return */
						UTF8_BADCHAR(0, utf_parse_blk.badcharstr, utf_parse_blk.badchartop, 0, NULL);
					else
						assertpro(FALSE);		/* Unknown error - no return */
				}
			} else
			{
				srcptr = src->str.addr;
				found_start = TRUE;				/* Know where to start looking  (at beginning) */
			}
			/* If we know where to start looking, start the scan - else we return not-found */
			if (found_start)
			{
				srclen = (int)(srctop - srcptr);
				assert(0 <= srclen);
				numpcs = 1;
				match = (char *)matchc(del->str.len, (uchar_ptr_t)del->str.addr,
						       srclen, (uchar_ptr_t)srcptr, &match_res, &numpcs);
				result = match_res ? first + match_res : 0;
			} else
			{
				DBGUTFC((stderr, "op_fnfind(%d) Start not found\n", findcnt));
			}
		}
	}
	DBGUTFC((stderr, "op_fnfind(%d): result returned for string 0x"lvaddr": %d\n", findcnt, src->str.addr, result));
	MV_FORCE_MVAL(dst, result);
	return result;
}
#else
#  include "utfcgr_trc.h"		/* Needed for op_fnzfind() DBGUTFC() macro below */
#endif /* UNICODE_SUPPORTED */

int4 op_fnzfind(mval *src, mval *del, mint first, mval *dst)
{
	mint	result;
	char	*match;
	int	match_res, numpcs;

	MV_FORCE_STR(src);
	MV_FORCE_STR(del);
	DBGUTFC((stderr, "\nop_fnzfind(%d): Called for string 0x"lvaddr" with first value: %d\n", ++findcnt, src->str.addr,
		 first));
	if (0 < first)
		first--;
	else
		first = 0;
	if (0 == del->str.len)
		result = first + 1;
	else if (src->str.len > first)
	{
		numpcs = 1;
		match = (char *)matchb(del->str.len, (uchar_ptr_t)del->str.addr,
				       src->str.len - first, (uchar_ptr_t)src->str.addr + first, &match_res, &numpcs);
		result = match_res ? first + match_res : 0;
	} else
		result = 0;
	DBGUTFC((stderr, "op_fnzfind(%d): result returned for string 0x"lvaddr": %d\n", findcnt, src->str.addr, result));
	MV_FORCE_MVAL(dst, result);
	return result;
}
