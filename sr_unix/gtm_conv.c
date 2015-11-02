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
#include "stringpool.h"
#include "gtm_caseconv.h"
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"

LITDEF mstr chset_names[CHSET_MAX_IDX_ALL] =
{ /* Supported character set (CHSET) codes for the 3-argument form of $ZCONVERT.
   *  NOTE: Update the *_CHSET_LEN macros below if new CHSETs are added.
   */
	{1, 1, "M"},	/* "M" should be the first CHSET (0th index of "chset_names" array). verify_chset() callers rely on this.
			 * $ZCONVERT doesn't support M, but I/O does */
	{5, 5, "UTF-8"},
	{6, 6, "UTF-16"},
	{8, 8, "UTF-16LE"},
	{8, 8, "UTF-16BE"},
	{5, 5, "ASCII"},
	{6, 6, "EBCDIC"},
	{6, 6, "BINARY"}
};
#define MIN_CHSET_LEN	1	/* minimum length of CHSET names */
#define MAX_CHSET_LEN	8	/* maximum length of CHSET names */

/* This array holds the ICU converter handles corresponding to the respective
 * CHSET name in the table chset_names[]
 */
GBLDEF	UConverter	*chset_desc[CHSET_MAX_IDX];
GBLDEF casemap_t casemaps[MAX_CASE_IDX] =
{ /* Supported case mappings and their disposal conversion routines for both $ZCHSET modes.
   * Note: since UTF-8 disposal functions for "U" and "L" are ICU "function pointers" rather
   * rather than their direct addresses, they are initialized in gtm_utf8_init() instead
   */
	{"U", &lower_to_upper, NULL           },
	{"L", &upper_to_lower, NULL           },
	{"T", NULL,            &gtm_strToTitle}
};

GBLREF	spdesc 		stringpool;

LITREF unsigned char 	lower_to_upper_table[];

error_def(ERR_ICUERROR);
error_def(ERR_MAXSTRLEN);

/* Routine to verify given parameter against supported CHSETs.
 * Valid arguments (case-insensitive):
 *	"M", "UTF-8", "UTF-16", "UTF-16LE" and "UTF-16BE"
 * Returns
 *	-1 (if invalid argument) or
 *	0  (if "M") or
 *	non-zero index to an entry of chset_names[] (if valid)
 */
int verify_chset(const mstr *parm)
{
	const mstr	*vptr, *vptr_top;
	char		mode[MAX_CHSET_LEN];

	if ((MIN_CHSET_LEN > parm->len) || (MAX_CHSET_LEN < parm->len))
		return -1; /* Parameter is smaller or larger than any possible CHSET */
	/* Make a translated copy of the parm */
	lower_to_upper((unsigned char *)mode, (unsigned char *)parm->addr, parm->len);
	/* See if any of our possibilities match */
	for (vptr = chset_names, vptr_top = vptr + CHSET_MAX_IDX_ALL; vptr < vptr_top; ++vptr)
	{
		if (parm->len == vptr->len &&
		    0 == memcmp(mode, vptr->addr, vptr->len))
			return (int)(vptr - chset_names); /* return the index */
	}
	return -1;
}

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

void callback_stop(const void* context, UConverterToUnicodeArgs *args, const char *codeUnits,
		int32_t length, UConverterCallbackReason reason, UErrorCode *pErrorCode)
{
	/* EMPTY BODY:
	 * By not resetting the pErrorCode, this routine returns to ICU routine directing
	 * it to stop and return immediately
	 */
}

UConverter* get_chset_desc(const mstr* chset)
{
	int 			chset_indx;
	UErrorCode		status;

	if ((0 >= (chset_indx = verify_chset(chset))) || (CHSET_MAX_IDX <= chset_indx))
		return NULL;
	if (NULL == chset_desc[chset_indx])
	{
		status = U_ZERO_ERROR;
		chset_desc[chset_indx] = ucnv_open(chset_names[chset_indx].addr, &status);
		if (U_FAILURE(status))
			rts_error(VARLSTCNT(3) ERR_ICUERROR, 1, status);	/* strange and unexpected ICU unhappiness */
		/* Initialize the callback for illegal/invalid characters, so that conversion
		 * stops at the first illegal character rather than continuing with replacement */
		status = U_ZERO_ERROR;
		ucnv_setToUCallBack(chset_desc[chset_indx], &callback_stop, NULL, NULL, NULL, &status);
		if (U_FAILURE(status))
			rts_error(VARLSTCNT(3) ERR_ICUERROR, 1, status);	/* strange and unexpected ICU unhappiness */
	}
	return chset_desc[chset_indx];
}

/* Startup initializations of conversion data */
void gtm_conv_init(void)
{
	assert(gtm_utf8_mode);
	/* Implicitly created CHSET descriptor for UTF-8 */
	get_chset_desc(&chset_names[CHSET_UTF8]);
	assert(NULL != chset_desc[CHSET_UTF8]);
	/* initialize the case conversion disposal functions */
	casemaps[0].u = u_strToUpper;
	casemaps[1].u = u_strToLower;
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
