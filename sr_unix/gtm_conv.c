/****************************************************************
 *								*
 *	Copyright 2006, 2012 Fidelity Information Services, Inc	*
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
#include "gtm_caseconv.h"
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"

GBLREF spdesc 		stringpool;
GBLREF UConverter	*chset_desc[CHSET_MAX_IDX];
GBLREF casemap_t	casemaps[MAX_CASE_IDX];

LITREF mstr		chset_names[CHSET_MAX_IDX_ALL];
LITREF unsigned char 	lower_to_upper_table[];

error_def(ERR_MAXSTRLEN);

/* Routine to verify given parameter against supported case conversion codes.
 * Valid arguments (case-insensitive):
 * 	"U", "L" and "T"
 * Returns
 * 	-1 (if invalid argument) or
 * 	index to an entry of casemaps[] (if valid)
 */
int verify_case(const mstr *parm)
{
	unsigned char	c;
	int		index;

	if (1 == parm->len)
	{
		c = lower_to_upper_table[*(uchar_ptr_t)parm->addr];
		if (!gtm_utf8_mode && 'T' == c)	/* title case is not supported in "M" mode */
			return -1;
		for (index = 0; index < MAX_CASE_IDX; ++index)
		{
			if (c == casemaps[index].code[0])
				return index;
		}
	}
	return -1;
}

int32_t gtm_strToTitle(UChar *dst, int32_t dstlen, const UChar *src, int32_t srclen,
		const char *locale, UErrorCode *status)
{
	return u_strToTitle(dst, dstlen, src, srclen, NULL, locale, status);
}

int gtm_conv(UConverter* from, UConverter* to, mstr *src, char* dstbuff, int* bufflen)
{
	char		*dstptr, *dstbase, *srcptr;
	const char	*ichset;
	int		dstlen, src_charlen, srclen;
	UErrorCode	status, status1;

	if (0 == src->len)
		return 0;
	if (NULL == dstbuff)
	{
		/* Compute the stringpool buffer space needed for conversion given that source
		 * is encoded in the ichset representation.  The ICU functions ucnv_getMinCharSize()
		 * and ucnv_getMaxCharSize() are used to compute the minimum and maximum number of
		 * bytes required per UChar if converted from/to ichset/ochset respectively
		 */
		src_charlen = (src->len / ucnv_getMinCharSize(from)) + 1; /* number of UChar's from ichset */
		dstlen = UCNV_GET_MAX_BYTES_FOR_STRING(src_charlen, ucnv_getMaxCharSize(to));
		dstlen = (dstlen > MAX_STRLEN) ? MAX_STRLEN : dstlen;
		ENSURE_STP_FREE_SPACE(dstlen);
		dstbase = (char *)stringpool.free;
	} else
	{
		dstbase = dstbuff;
		dstlen = *bufflen;
	}
	srcptr = src->addr;
	srclen = (int)src->len;
	dstptr = dstbase;
	status = U_ZERO_ERROR; /* initialization to "success" is required by ICU */
	ucnv_convertEx(to, from, &dstptr, dstptr + dstlen, (const char**)&srcptr, srcptr + srclen,
		NULL, NULL, NULL, NULL, TRUE, TRUE, &status);
	if (U_FAILURE(status))
	{
		if (U_BUFFER_OVERFLOW_ERROR == status)
		{	/* translation requires more space than the maximum allowed GT.M string size */
			if (NULL == dstbuff)
				rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
			else
			{
				/* Insufficient buffer passed. Return the required buffer length */
				src_charlen = (srclen / ucnv_getMinCharSize(from)) + 1;
				*bufflen = UCNV_GET_MAX_BYTES_FOR_STRING(src_charlen, ucnv_getMaxCharSize(to));
				return -1;
			}
		}
		status1 = U_ZERO_ERROR;
		ichset = ucnv_getName(from, &status1);
		assert(U_SUCCESS(status1));
		UTF8_BADCHAR(1,(unsigned char *) (srcptr - 1), NULL,STRLEN(ichset), ichset);
	}
	return (int) (dstptr - dstbase);
}
