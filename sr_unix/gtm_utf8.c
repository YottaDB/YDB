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

int utf8_len(mstr* str)
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
				UTF8_BADCHAR(0, ptr, ptrtop, 0, NULL);
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
		rts_error(VARLSTCNT(6) ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0], chset_len, chset);
	else
		rts_error(VARLSTCNT(6) ERR_BADCHAR, 4, (outstr - &errtxt[0]), &errtxt[0], LEN_AND_LIT(UTF8_NAME));
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
	if (U32_LT_LF == lt_index && 1 < len)
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
	else
		return len;		/* no line terminator so return it all */
}
