/****************************************************************
 *								*
 * Copyright (c) 2006-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "error.h"
#include "util.h"
#include "gtm_icu_api.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	badchar_inhibit;
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	void		(*stx_error_fptr)(int in_error, ...);	/* Function pointer for stx_error() so gtm_utf8.c can avoid pulling
								 * in stx_error() in gtmsecshr.
								 */
GBLREF	void		(*show_source_line_fptr)(boolean_t warn); /* Func ptr for show_source_line() for same reason as above */

error_def(ERR_BADCHAR);

/* Return UTF8 length of mstr string in UTF8 characters */
int utf8_len(mstr* str)
{
	return utf8_len_real(err_rts, str);
}

/* This is the same as "utf8_len" except that it invokes UTF8_BADCHAR_STX macro which does a stx_error instead of rts_error
 * when an invalid UTF8 character is detected in the string (and badchar_inhibit is not enabled).
 * If UTF8_BADCHAR_STX is invoked, this function returns a -1 signalling a parse error.
 */
int utf8_len_stx(mstr* str)
{
	return utf8_len_real(err_stx, str);
}

/* This is the same as "utf8_len" except that it invokes UTF8_BADCHAR_DEC macro which does a dec_err instead of rts_error.
 * Note only one "error" is raised for any given string and we return the length as best we can with the broken string.
 */
int utf8_len_dec(mstr* str)
{
	return utf8_len_real(err_dec, str);
}

/* The routine that does the actual work of determining the length and responding appropriately in the event an invalid
 * UTF8 character is detected.
 */
STATICFNDEF int utf8_len_real(utf8_err_type err_type, mstr* str)
{
	int		charlen, bytelen;
	char		*ptrtop, *ptr;
	boolean_t	err_raised;

	assert(gtm_utf8_mode);
	ptr = str->addr;
	ptrtop = ptr + str->len;
	charlen = 0;
	err_raised = FALSE;
	if (!badchar_inhibit)
	{
		for (; ptr < ptrtop; charlen++, ptr += bytelen)
		{
			if (!UTF8_VALID(ptr, ptrtop, bytelen))
			{
				switch(err_type)
				{
					case err_rts:
						UTF8_BADCHAR(0, ptr, ptrtop, 0, NULL);
						break;			/* Never get here but keeps compiler happy */
					case err_stx:
						UTF8_BADCHAR_STX(0, ptr, ptrtop, 0, NULL);
						return -1;
					case err_dec:
						if (!err_raised)
						{
							UTF8_BADCHAR_DEC(0, ptr, ptrtop, 0, NULL);
							err_raised = TRUE;
						}
						bytelen = 1;		/* Assume only one char is broken */
						break;
					default:
						assertpro(FALSE /* Invalid error type */);
				}
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

/* Similar to utf8_len() except it operates on a given string instead of an mval and does not observe badchar_inhibit.
 * String must be valid or error is raised.
 */
int utf8_len_strict(unsigned char* ptr, int len)
{
	int		charlen, bytelen;
	unsigned char	*ptrtop;

	ptrtop = ptr + len;
	for (charlen = 0; ptr < ptrtop; charlen++, ptr += bytelen)
	{
		if (!UTF8_VALID(ptr, ptrtop, bytelen))
			UTF8_BADCHAR(0, ptr, ptrtop, 0, NULL);
	}
	assert(ptr == ptrtop);
	return charlen;
}

/* Returns the total display column width of a UTF-8 string given its address and byte length.
 * The third parameter (strict) is used to specify how both illegal characters should be handled.
 * The fourth parameter (nonprintwidth) is to specify what width to give for unprintable characters.
 *	It is currently 0 if coming through $ZWIDTH and 1 if coming through util_output (for historical reasons).
 * If strict is TRUE, this routine
 * 	- triggers BADCHAR error if it encounters any illegal characters irrespective of VIEW BADCHAR setting.
 * If strict is FALSE, this routine
 * 	- does NOT do BADCHAR check.
 *	- treats illegal characters as unprintable characters (for width).
 */
int gtm_wcswidth(unsigned char* ptr, int len, boolean_t strict, int nonprintwidth)
{
	int		strwidth, cwidth;
	uint4		ch;
	unsigned char	*ptrtop, *ptrnext;

	assert(gtm_utf8_mode);
	ptrtop = ptr + len;
	for (strwidth = 0; ptr < ptrtop; ptr = ptrnext)
	{
		ptrnext = UTF8_MBTOWC(ptr, ptrtop, ch);
		if (WEOF != ch && -1 != (cwidth = UTF8_WCWIDTH(ch)))
			strwidth += cwidth;
		else if (strict && (WEOF == ch))
			UTF8_BADCHAR(0, ptr, ptrtop, 0, NULL);
		else
			strwidth += nonprintwidth;
	}
	assert(ptr == ptrtop);
	return strwidth;
}

/* Returns the display column width of a character given its code point. This function
 * returns -1 for control characters and 0 for non-spacing (combining) characters.
 *
 * NOTEs:
 * We are not using libc's wcwidth() due to its inconsistent behavior across different
 * platforms and its incorrect behavior for several characters (even on Linux).
 *
 * ICU does not provide a direct API for display width, however, it does provide API
 * for the property "East Asian Width" specified in the "Unicode Standard Annex #11"
 * which provides guidelines to determine the width for the entire Unicode repertoire.
 *
 * Using "East Asian Width" and "General Category" properties. gtm_wcwidth() determines
 * the column width as below:
 * 	- SOFT-HYPHEN is a special format control character with a width of 1.
 *	- Non-spacing combining marks and Enclosing combining marks (Unicode general
 *	  category codes 'Mn' and 'Me') have a column width of 0. Note that Combing spacing
 *	  marks (General Category 'Mc') occupy a column width of 1.
 *	- Conjoining Hangul Jamos (i.e. vowels and trailing consonants between U+1160 -
 *	  U+11FF) have a column with of 0. They are like the combining marks in that they
 *	  attach to their previous characters (although they categorized as letters).
 *	- All wide characters (East Asian Width - Wide (W) and Full-Width (F)) have a
 *	  column width of 2 and all narrow characters (East Asian Width - Narrow (Na)
 *	  and Half-Width (H)) have a column width of 1.
 *	- All characters (with East Asian Width - Neutral (N) and Ambiguous (A)) have a
 *	  column width of 1.
 * 	- All other non-printable (control characters) and unassigned code points (empty blocks)
 * 	  have a width -1.
 */
int gtm_wcwidth(wint_t code)
{
	UCharCategory		gc;	/* General category as defined by the Unicode standard */
	UEastAsianWidth		ea;
	UHangulSyllableType	hst;

	assert(gtm_utf8_mode);
	if (0x00ad == code) /* SOFT-HYPHEN, a special format control character */
		return 1;
	gc = (UCharCategory)u_getIntPropertyValue((UChar32)code, UCHAR_GENERAL_CATEGORY);
	if (U_NON_SPACING_MARK == gc || U_ENCLOSING_MARK == gc || /* combining marks (Mn, Me) */
		U_FORMAT_CHAR == gc || /* all other format control (Cf) characters */
		U_HST_VOWEL_JAMO == (hst = (UHangulSyllableType)u_getIntPropertyValue((UChar32)code,
			UCHAR_HANGUL_SYLLABLE_TYPE)) ||
		U_HST_TRAILING_JAMO == hst) /* conjoining hangul jamos (in Korean) */
	{
		return 0;
	}
	if (U_ISPRINT((UChar32)code))
	{
		ea = (UEastAsianWidth)u_getIntPropertyValue((UChar32)code, UCHAR_EAST_ASIAN_WIDTH);
		return (U_EA_FULLWIDTH == ea || U_EA_WIDE == ea) ? 2 : 1;
	}
	return -1;
}

/* This function issues a BADCHAR error and prints the sequences of bytes that comprise the bad multi-byte character.
 * If "len" is 0, the function determines how many bytes this multi-byte character is comprised of and prints all of it.
 * If "len" is non-zero, the function prints "len" number of bytes from "str" in the error message.
 */
void utf8_badchar(int len, unsigned char *str, unsigned char *strtop, int chset_len, unsigned char *chset)
{
	DCL_THREADGBL_ACCESS;
	SETUP_THREADGBL_ACCESS;

	assert(!TREF(compile_time));
	utf8_badchar_real(err_rts, len, str, strtop, chset_len, chset);
	return;
}

/* This function is the same as "utf8_badchar" except that it does a "stx_error" instead of "rts_error". This helps
 * to identify the line in the M program that has the compile time error.
 */
void utf8_badchar_stx(int len, unsigned char *str, unsigned char *strtop, int chset_len, unsigned char *chset)
{
	DCL_THREADGBL_ACCESS;
	SETUP_THREADGBL_ACCESS;

	assert(!TREF(compile_time));
	utf8_badchar_real(err_stx, len, str, strtop, chset_len, chset);
	return;
}

/* This function is the same as "utf8_badchar" except that it does a "dec_err()" instead of "rts_error". This helps
 * to identify the line in the M program that has the compile time error but unlike stx_error(), it does not remove
 * the relevant generated code and replace it with a runtime OC_RTERROR triple. This is because the runtime code does
 * the same level of checking as the compiler does so can detect the error "normally" on its own - no need for the
 * complication with trying to cut out the right set of triples. We can just put out the compiler message and let it
 * be.
 */
void utf8_badchar_dec(int len, unsigned char *str, unsigned char *strtop, int chset_len, unsigned char *chset)
{
	utf8_badchar_real(err_dec, len, str, strtop, chset_len, chset);
	return;
}

/* This function issues a BADCHAR error and prints the sequences of bytes that comprise the bad multi-byte character.
 * If "len" is 0, the function determines how many bytes this multi-byte character is comprised of and prints all of it.
 * If "len" is non-zero, the function prints "len" number of bytes from "str" in the error message.
 * This is the work-horse routine for the 3 above variants of utf8_badchar*(). The differences are in how the error
 * is delivered and what happens afterwards. For the 3 options:
 *
 *    err_rts    - uses rts_error() to raise the error
 *    err_stx    - uses stx_error to raise the error
 *    err_dec	 - uses dec_err to raise the error
 */
STATICFNDEF void utf8_badchar_real(utf8_err_type err_type, int len, unsigned char *str, unsigned char *strtop, int chset_len,
				   unsigned char *chset)
{
	unsigned char 	*strptr, *strend, *outstr;
	unsigned char	errtxt[OUT_BUFF_SIZE];
	int		tmplen;

	DCL_THREADGBL_ACCESS;
	SETUP_THREADGBL_ACCESS;

	assert(gtm_utf8_mode);
	if (0 == len)
	{	/* Determine the maximal length (upto 4 bytes) of the invalid byte sequence */
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
	if (0 < len)		/* do not include the last comma */
		outstr--;
	if (err_dec == err_type)
	{
		assert(NULL != show_source_line_fptr);
		(*show_source_line_fptr)(TRUE);	/* Prints errant source line and pointer to where parsing detected the error */
	}
	if (0 < chset_len)
	{
		switch(err_type)
		{
			case err_rts:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0],
					      chset_len, chset);
				break;		/* Never get here but keeps compiler happy */
			case err_stx:
				assert(NULL != stx_error_fptr);
				(*stx_error_fptr)(ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0], chset_len, chset);
				break;
			case err_dec:
				dec_err(VARLSTCNT(6) (TREF(compile_time) ? MAKE_MSG_TYPE(ERR_BADCHAR, WARNING)  : ERR_BADCHAR),
					4, (outstr - &errtxt[0]), &errtxt[0], chset_len, chset);
				break;
			default:
				assertpro(FALSE /* Invalid error type */);
		}
	} else
	{

		switch(err_type)
		{
			case err_rts:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0],
					      LEN_AND_LIT(UTF8_NAME));
				break;		/* Never get here but keeps compiler happy */
			case err_stx:
				assert(NULL != stx_error_fptr);
				(*stx_error_fptr)(ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0], LEN_AND_LIT(UTF8_NAME));
				break;
			case err_dec:
				dec_err(VARLSTCNT(6) (TREF(compile_time) ? MAKE_MSG_TYPE(ERR_BADCHAR, WARNING) : ERR_BADCHAR),
					4, (outstr - &errtxt[0]), &errtxt[0], LEN_AND_LIT(UTF8_NAME));
				break;
			default:
				assertpro(FALSE /* Invalid error type */);
		}
	}
}

/* This function scans the string from the beginning and stops the moment it finds an invalid byte sequence.
 * It null-terminates the string from then onwards until the end.
 */
unsigned char *gtm_utf8_trim_invalid_tail(unsigned char *str, int len)
{
	unsigned char	*ptrend, *ptr;
	int		bytelen;

	ptr = str;
	ptrend = str + len;
	while (ptr < ptrend)
	{
		if (UTF8_VALID(ptr, ptrend, bytelen))
			ptr += bytelen;
		else
			break;
	}
	for ( ; ptr < ptrend; ptr++)
		*ptr = '\0';
	return ptr;
}

/* Remove the trailing line terminator from buffer.
 * buffer	line in ICU UChar format
 * len		length of line as number of UChar characters
 * 			as given by u_strlen()
 * Returns	number of characters after removing line terminator
 */
int trim_U16_line_term(UChar *buffer, int len)
{
	int	lt_index;
	UChar32	uc32_cp;

	if (0 == len)
		return 0;		/* zero length string */
	U16_GET(buffer, 0, len - 1, len, uc32_cp);
	for (lt_index = 0; u32_line_term[lt_index]; lt_index++)
		if (uc32_cp == u32_line_term[lt_index])
			break;
	if ((U32_LT_LF == lt_index) && (1 < len))
	{
		U16_GET(buffer, 0, len - 2, len, uc32_cp);
		if (u32_line_term[U32_LT_CR] == uc32_cp)
			len--;		/* trim both CR and LF */
	}
	if (U32_LT_LAST >= lt_index)
	{
		buffer[len - 1] = 0;
		return (len - 1);
	}
	return len;		/* no line terminator so return it all */
}

boolean_t valid_utf_string(const mstr *str)
{
	int		charlen, bytelen;
	char		*ptrtop, *ptr;

	ptr = str->addr;
	ptrtop = ptr + str->len;
	charlen = 0;

	for (; ptr < ptrtop; charlen++, ptr += bytelen)
	{
		if (!UTF8_VALID(ptr, ptrtop, bytelen))
		{	/* Emit a warning if there is an issue*/
			UTF8_BADCHAR_DEC(0, ptr, ptrtop, 0, NULL);
			return FALSE;
		}
	}
	return TRUE;
}
