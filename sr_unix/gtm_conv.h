/****************************************************************
 *                                                              *
 *	Copyright 2006, 2012 Fidelity Information Services, Inc	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef GTM_CONV_H
#define GTM_CONV_H
#include "gtm_icu_api.h"

#define MAX_CASE_IDX	3		/* maximum number of case conversions supported */
#define MAX_ZCONVBUFF	(8 * 1024)	/* temporary buffer size used in case conversion */

#define MIN_CHSET_LEN	1		/* minimum length of CHSET names */
#define MAX_CHSET_LEN	8		/* maximum length of CHSET names */

int verify_chset(const mstr *parm);
int verify_case(const mstr *parm);
UConverter* get_chset_desc(const mstr *chset);
int gtm_conv(UConverter* from, UConverter* to, mstr* src, char* dstbuff, int* bufflen);

typedef void 	(*m_casemap_t)(uchar_ptr_t, uchar_ptr_t, int4);
typedef int32_t	(*u_casemap_t)(UChar *dest, int32_t destCapacity, const UChar *src,
			       int32_t srcLength, const char *locale, UErrorCode *pErrorCode);

/* An interlude routine for title case to have the same signature as u_strToUpper/u_strToLower */
int32_t gtm_strToTitle(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength,
		       const char *locale, UErrorCode *pErrorCode);

/* descriptor for case mapping */
typedef struct
{
	const char*	code;	/* case conversion code - "U","L","T" */
	m_casemap_t	m;	/* conversion routine for "M" mode */
	u_casemap_t	u;	/* conversion routine for "UTF-8" mode */
} casemap_t;

/* Call back function for ICU to stop at illegal/invalid characters and return with error */
void callback_stop(const void* context, UConverterToUnicodeArgs *args, const char *codeUnits,
		   int32_t length, UConverterCallbackReason reason, UErrorCode *pErrorCode);

#endif /* GTM_CONV_H */
