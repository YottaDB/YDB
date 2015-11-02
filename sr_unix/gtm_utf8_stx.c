/****************************************************************
 *								*
 *	Copyright 2006, 2007 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "error.h"
#include "util.h"
#include "gtm_icu_api.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	badchar_inhibit;
GBLREF	boolean_t	gtm_utf8_mode;

error_def(ERR_BADCHAR);

/* This is the same as "utf8_len" except that it invokes UTF8_BADCHAR_STX macro which does a stx_error instead of rts_error.
 * If UTF8_BADCHAR_STX is invoked, this function returns a -1 signalling a parse error.
 */
int utf8_len_stx(mstr* str)
{
	int	charlen, bytelen;
	char	*ptrtop, *ptr;

	assert(gtm_utf8_mode);
	ptr = str->addr;
	ptrtop = ptr + str->len;
	charlen = 0;
	if (!badchar_inhibit)
	{
		for (; ptr < ptrtop; charlen++, ptr += bytelen)
		{
			if (!UTF8_VALID(ptr, ptrtop, bytelen))
			{
				UTF8_BADCHAR_STX(0, ptr, ptrtop, 0, NULL);
				return -1;
			}
		}
	} else
	{
		for (; ptr < ptrtop; charlen++)
			ptr = (char *)UTF8_MBNEXT(ptr, ptrtop);
	}
	assert(ptr == ptrtop);
	str->char_len = charlen;
	return charlen;
}

/* This function is the same as "utf8_badchar" except that it does a "stx_error" instead of "rts_error". This helps
 * to identify the line in the M program that has the compile time error.
 */
void utf8_badchar_stx(int len, unsigned char *str, unsigned char *strtop, int chset_len, unsigned char *chset)
{
	unsigned char 	*strptr, *strend, *outstr;
	unsigned char	errtxt[OUT_BUFF_SIZE];
	int		tmplen;

	assert(gtm_utf8_mode);
	if (len == 0)
	{ /* Determine the maximal length (upto 4 bytes) of the invalid byte sequence */
		for (strend = str; len <= 4 && strend < strtop; ++strend, ++len)
		{
			if (UTF8_VALID(strend, strtop, tmplen))
				break;
		}
	} else
		strend = str + len;
	strptr = str;
	outstr = &errtxt[0];
	for (; strptr < strend; ++strptr, ++outstr)
	{
		outstr = (unsigned char*)i2asc((uchar_ptr_t)outstr, *strptr);
		*outstr = ',';
	}
	if (len > 0) /* do not include the last comma */
		outstr--;
	if (chset_len > 0)
		stx_error(ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0], chset_len, chset);
	else
		stx_error(ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0], LEN_AND_LIT(UTF8_NAME));
}
