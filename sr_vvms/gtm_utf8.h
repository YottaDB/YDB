/****************************************************************
 *                                                              *
 *      Copyright 2006 Fidelity Information Services, Inc 	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef GTM_UTF8_H
#define GTM_UTF8_H

/* Define types for use by compilation in VMS in code paths that will never be
   used. This was the preferred method of making the Unicode modified UNIX code
   work in VMS rather than butchering it with ifdefs making maintenance more
   difficult. 9/2006 SE
*/

#define UTF8_VALID(mbptr, ptrend, bytelen) (bytelen = -1, FALSE)
#define UTF16BE_VALID(mbptr, ptrend, bytelen) (bytelen = -1, FALSE)
#define UTF16LE_VALID(mbptr, ptrend, bytelen) (bytelen = -1, FALSE)
#define UTF8_MBFOLLOW(mbptr) (-1)
#define UTF16BE_MBFOLLOW(mbptr, ptrend) (-1)
#define UTF16LE_MBFOLLOW(mbptr, ptrend) (-1)
#define UTF8_WCTOMB(codepoint, mbptr) mbptr
#define UTF16BE_WCTOMB(codepoint, mbptr) mbptr
#define UTF16LE_WCTOMB(codepoint, mbptr) mbptr
#define UTF8_MBTOWC(mbptr, ptrend, codepoint) (-1)
#define UTF16BE_MBTOWC(mbptr, ptrend, codepoint) (-1)
#define UTF16LE_MBTOWC(mbptr, ptrend, codepoint) (-1)
#define U_VALID_CODE(codepoint) (FALSE)
#define UTF16BE_BOM          '\0'
#define UTF16BE_BOM_LEN      1
#define UTF16LE_BOM          '\0'
#define UTF16LE_BOM_LEN      1
#define UTF8_BOM             '\0'
#define UTF8_BOM_LEN         1

#define GTM_MB_LEN_MAX		1	/* VMS does not support unicode so no multiple byte chars */

int	utf8_len_strict(unsigned char* ptr, int len);
void    utf8_badchar(int len, unsigned char* str, unsigned char *strtop, int chset_len, unsigned char* chset);

#endif /* GTM_UTF8_H */
