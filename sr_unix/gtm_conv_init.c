/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gtm_caseconv.h"
#include "gtm_utf8.h"
#include "gtm_conv.h"

GBLREF UConverter	*chset_desc[CHSET_MAX_IDX];
GBLREF casemap_t	casemaps[MAX_CASE_IDX];
GBLREF u_casemap_t	gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */

LITREF mstr		chset_names[CHSET_MAX_IDX_ALL];

/* Note these routines are separated from gtm_conv.c to avoid pulling into gtmsecshr all the stuff the conversion modules use */

error_def(ERR_ICUERROR);

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
	casemaps[2].u = gtm_strToTitle_ptr;
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

void callback_stop(const void* context, UConverterToUnicodeArgs *args, const char *codeUnits,
		int32_t length, UConverterCallbackReason reason, UErrorCode *pErrorCode)
{
	/* EMPTY BODY:
	 * By not resetting the pErrorCode, this routine returns to ICU routine directing
	 * it to stop and return immediately
	 */
}
