/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "op.h"
#include "stringpool.h"
#include "gtm_icu_api.h"
#include "gtm_conv.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	spdesc 		stringpool;
GBLREF	casemap_t	casemaps[];

error_def(ERR_BADCASECODE);
error_def(ERR_BADCHSET);
error_def(ERR_ICUERROR);
error_def(ERR_MAXSTRLEN);
error_def(ERR_INVFCN);
error_def(ERR_TEXT);

#define RELEASE_IF_NOT_LOCAL(ptr, local) ((ptr) != (local)) ? (free(ptr), (ptr = NULL)) : ptr;

/**************************************************************************************************
 * Routine to perform string-level case conversion to "upper", "lower" and "title" case.
 * Since ICU only supports API using UTF-16 representation, case conversion of UTF-8 strings involves
 * encoding conversion as described below:
 * 	1. First, the UTF-8 source string is converted to UTF-16 representation (u_strFromUTF8())
 * 	   which is stored in a local buffer of size MAX_ZCONVBUFF. If this space is not sufficient,
 * 	   we try to allocate it in heap.
 * 	2. Since case conversion may expand the string, we compute the desired space required by
 * 	   preflighting the ICU case conversion API and then allocate the space before performing
 * 	   the real conversion.
 * 	3. Translating the converted UTF-16 string back to UTF-8 is done in stringpool (with similar
 * 	   preflighting to compute the required space.
 * NOTE:
 * 	Malloc is used only if the size exceeds 2K characters (a very unlikely situation esp. with
 * 	case conversion).
 *
 ***************************************************************************************************/
void	op_fnzconvert2(mval *src, mval *kase, mval *dst)
{
	int		index;
	int32_t		src_ustr_len, src_chlen, dst_chlen, ulen, dstlen = 0;
	UErrorCode	status;
	char		*dstbase;
	UChar		src_ustr[MAX_ZCONVBUFF], dst_ustr[MAX_ZCONVBUFF], *src_ustr_ptr, *dst_ustr_ptr;

	MV_FORCE_STR(kase);
	if (-1 == (index = verify_case(&kase->str)))
		rts_error(VARLSTCNT(4) ERR_BADCASECODE, 2, kase->str.len, kase->str.addr);

	MV_FORCE_STR(src);
	/* allocate stringpool */
	if (!gtm_utf8_mode)
	{
		dstlen = src->str.len;
		ENSURE_STP_FREE_SPACE(dstlen);
		dstbase = (char *)stringpool.free;
		assert(NULL != casemaps[index].m);
		(*casemaps[index].m)((unsigned char *)dstbase, (unsigned char *)src->str.addr, dstlen);
	} else if (0 != src->str.len)
	{
		MV_FORCE_LEN_STRICT(src);
		if (2 * src->str.char_len <= MAX_ZCONVBUFF)
		{ /* Check if the stack buffer is sufficient considering the worst case where all
		     characters are surrogate pairs, each of which needs 2 UChars */
			src_ustr_ptr = src_ustr;
			src_ustr_len = MAX_ZCONVBUFF;
		} else
		{ /* Fake the conversion from UTF-8 to UTF-16 to compute the required number of UChars */
			status = U_ZERO_ERROR;
			u_strFromUTF8(NULL, 0, &src_ustr_len, src->str.addr, src->str.len, &status);
			if (U_FAILURE(status))
			{	/* Since there is no ustring.h API to return the actual illegal sequence,
				 * we need to search the UTF-8 source to identify the BADCHAR sequence
				 * might get buffer overflow on the way to ensuring enough space
				 */
				if (U_FAILURE(status) && (U_BUFFER_OVERFLOW_ERROR != status))
				{
					if (U_ILLEGAL_CHAR_FOUND == status || U_INVALID_CHAR_FOUND == status)
						utf8_len_strict((unsigned char *)src->str.addr, src->str.len);
					rts_error(VARLSTCNT(3) ERR_ICUERROR, 1, status);	/* ICU said bad, we say good */
				}
			}
			src_ustr_ptr = (UChar*)malloc(src_ustr_len * SIZEOF(UChar));
		}
		/* Convert UTF-8 src to UTF-16 (UChar*) representation */
		status = U_ZERO_ERROR;
		u_strFromUTF8(src_ustr_ptr, src_ustr_len, &src_chlen, src->str.addr, src->str.len, &status);
		if (U_FAILURE(status))
		{
			RELEASE_IF_NOT_LOCAL(src_ustr_ptr, src_ustr);
			if (U_ILLEGAL_CHAR_FOUND == status || U_INVALID_CHAR_FOUND == status)
				utf8_len_strict((unsigned char *)src->str.addr, src->str.len);	/* to report BADCHAR error */
			rts_error(VARLSTCNT(3) ERR_ICUERROR, 1, status); /* ICU said bad, we say good or don't recognize error*/
		}
		/* Next, fake conversion to compute the required buffer size (aka preflighting in ICU) */
		status = U_ZERO_ERROR;
		dst_chlen = (*casemaps[index].u)(NULL, 0, src_ustr_ptr, src_chlen, NULL, &status);
		assert(U_BUFFER_OVERFLOW_ERROR == status);
		if (dst_chlen >  MAX_ZCONVBUFF) /* conversion increases the string length, allocate in heap instead */
			dst_ustr_ptr = (UChar*)malloc(dst_chlen * SIZEOF(UChar));
		else
			dst_ustr_ptr = dst_ustr;
		/* Now, perform the real conversion with sufficient buffers */
		status = U_ZERO_ERROR;
		dst_chlen = (*casemaps[index].u)(dst_ustr_ptr, dst_chlen, src_ustr_ptr, src_chlen, NULL, &status);
		if (U_FAILURE(status))
			GTMASSERT;
		RELEASE_IF_NOT_LOCAL(src_ustr_ptr, src_ustr);
		/* Fake the conversion from UTF-16 to UTF-8 to compute the required buffer size */
		status = U_ZERO_ERROR;
		dstlen = 0;
		u_strToUTF8(NULL, 0, &dstlen, dst_ustr_ptr, dst_chlen, &status);
		assert(U_BUFFER_OVERFLOW_ERROR == status || U_SUCCESS(status));
		if (MAX_STRLEN < dstlen)
		{
			RELEASE_IF_NOT_LOCAL(dst_ustr_ptr, dst_ustr);
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
		}
		ENSURE_STP_FREE_SPACE(dstlen);
		dstbase = (char *)stringpool.free;
		status = U_ZERO_ERROR;
		u_strToUTF8(dstbase, dstlen, &ulen, dst_ustr_ptr, dst_chlen, &status);
		if (U_FAILURE(status))
			rts_error(VARLSTCNT(3) ERR_ICUERROR, 1, status); /* ICU said bad, but same call above just returned OK */
		if (ulen != dstlen)
			GTMASSERT;
		RELEASE_IF_NOT_LOCAL(dst_ustr_ptr, dst_ustr);
	}
	MV_INIT_STRING(dst, dstlen, dstbase);
	stringpool.free += dstlen;
}

void	op_fnzconvert3(mval *src, mval* ichset, mval* ochset, mval* dst)
{
	UConverter	*from, *to;
	int		dstlen;

	MV_FORCE_STR(src);
	if (!gtm_utf8_mode)
	{ /* Unicode not enabled, report error rather than silently ignoring the conversion */
		rts_error(VARLSTCNT(6) ERR_INVFCN, 0, ERR_TEXT, 2,
			LEN_AND_LIT("Three-argument form of $ZCONVERT() is not allowed in the current $ZCHSET"));
	}
	MV_FORCE_STR(ichset);
	MV_FORCE_STR(ochset);
	/* The only supported names are: "UTF-8", "UTF-16", "UTF-16LE" and "UTF-16BE */
	if (NULL == (from = get_chset_desc(&ichset->str)))
		rts_error(VARLSTCNT(4) ERR_BADCHSET, 2, ichset->str.len, ichset->str.addr);
	if (NULL == (to = get_chset_desc(&ochset->str)))
		rts_error(VARLSTCNT(4) ERR_BADCHSET, 2, ochset->str.len, ochset->str.addr);

	dstlen = gtm_conv(from, to, &src->str, NULL, NULL);
	assert(-1 != dstlen);
	MV_INIT_STRING(dst, dstlen, stringpool.free);
	stringpool.free += dst->str.len;
}
