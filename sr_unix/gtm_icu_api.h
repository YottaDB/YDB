/****************************************************************
 *								*
 *	Copyright 2006, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ICU_API_H
#define ICU_API_H

#define U_DISABLE_RENAMING 1	/* required by ICU to disable renaming */

#define u_isalpha			(*u_isalpha_ptr)
#define u_islower			(*u_islower_ptr)
#define u_isupper			(*u_isupper_ptr)
#define u_istitle			(*u_istitle_ptr)
#define u_iscntrl			(*u_iscntrl_ptr)
#define u_ispunct			(*u_ispunct_ptr)
#define u_isdigit			(*u_isdigit_ptr)
#define u_isspace			(*u_isspace_ptr)
#define u_isblank			(*u_isblank_ptr)
#define u_isprint			(*u_isprint_ptr)
#define u_getIntPropertyValue		(*u_getIntPropertyValue_ptr)
#define u_strFromUTF8			(*u_strFromUTF8_ptr)
#define u_strToUTF8			(*u_strToUTF8_ptr)
#define u_strToLower			(*u_strToLower_ptr)
#define u_strToUpper			(*u_strToUpper_ptr)
#define u_strToTitle			(*u_strToTitle_ptr)
#define u_strlen			(*u_strlen_ptr)
#define u_toupper			(*u_toupper_ptr)
#define u_finit				(*u_finit_ptr)
#define u_fgets				(*u_fgets_ptr)
#define u_fclose			(*u_fclose_ptr)
#define u_feof				(*u_feof_ptr)
#define ucnv_open			(*ucnv_open_ptr)
#define ucnv_close			(*ucnv_close_ptr)
#define ucnv_convertEx			(*ucnv_convertEx_ptr)
#define ucnv_getMaxCharSize		(*ucnv_getMaxCharSize_ptr)
#define ucnv_getMinCharSize		(*ucnv_getMinCharSize_ptr)
#define ucnv_setToUCallBack		(*ucnv_setToUCallBack_ptr)
#define ucnv_getName			(*ucnv_getName_ptr)
#define u_charType			(*u_charType_ptr)

#include <unicode/uchar.h>
#include <unicode/ucnv.h>
#include <unicode/ustdio.h>
#include <unicode/ustring.h>

LITREF UChar32 		u32_line_term[];

void gtm_icu_init(void);
void gtm_conv_init(void);

GBLREF	boolean_t	is_gtm_chset_utf8;
#define	GTM_ICU_INIT_IF_NEEDED	if (is_gtm_chset_utf8) gtm_icu_init();

#endif /* ICU_API_H */
