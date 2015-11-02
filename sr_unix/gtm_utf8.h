/****************************************************************
 *								*
 *	Copyright 2006, 2010 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_UTF8_H
#define GTM_UTF8_H

#include <wctype.h>
#include <wchar.h>

/*
 *=======================================================================================================================
 * UTF-8 BIT DISTRIBUTION:
 *=======================================================================================================================
 *  Code range	    Scalar value		UTF-8					Notes
 *  hexadecimal	    binary			binary
 *  -----------------------------------------------------------------------------------------------------------------------
 *  000000-00007F   0xxxxxxx			0xxxxxxx				ASCII equivalence range;
 *		    seven x			seven x					byte begins with zero
 *
 *  000080-0007FF   00000xxx xxxxxxxx		110xxxxx 10xxxxxx			first byte begins with 110,
 *		    three x, eight x		five x, six x				the following byte begins with 10.
 *
 *  000800-00FFFF   xxxxxxxx xxxxxxxx		1110xxxx 10xxxxxx 10xxxxxx		first byte begins with 1110,
 *		    eight x, eight x		four x, six x, six x			the following bytes begin with 10.
 *
 *  010000-10FFFF   000xxxxx xxxxxxxx xxxxxxxx	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx	First byte begins with 11110,
 *		    five x, eight x, eight x	three x, six x, six x, six x		the following bytes begin with 10
 *
 *  ====================================================================================================================
 *  Codepoint  Codepoint binary	      UTF-8 binary			    UTF-8 hex
 *  hex
 *  --------------------------------------------------------------------------------------------------------------------
 *  000000     0		      0					    00		# 1-byte UTF-8 encoding BEGIN
 *  00007F     1111111		      1111111				    7F		# 1-byte UTF-8 encoding END
 *
 *  000080     00010000000	      11000010 10000000			    C2 80	# 2-byte UTF-8 encoding BEGIN
 *  0007FF     11111111111	      11011111 10111111			    DF BF	# 2-byte UTF-8 encoding END
 *
 *  000800     0000100000000000	      11100000 10100000 10000000	    E0 A0 80	# 3-byte UTF-8 encoding BEGIN
 *  00D7FF     1101011111111111	      11101101 10011111 10111111	    ED 9F BF	# 3-byte UTF-8 encoding PAUSE
 *
 *  00D800     1101100000000000	      11101101 10100000 10000000	    ED A0 80	# surrogate invalid range BEGIN
 *  00DFFF     1101111111111111	      11101101 10111111 10111111	    ED BF BF	# surrogate invalid range END
 *
 *  00E000     1110000000000000	      11101110 10000000 10000000	    EE 80 80	# 3-byte UTF-8 encoding RESUME
 *  00FFFF     1111111111111111	      11101111 10111111 10111111	    EF BF BF	# 3-byte UTF-8 encoding END
 *
 *  010000     000010000000000000000  11110000 10010000 10000000 10000000   F0 90 80 80 # 4-byte UTF-8 encoding BEGIN
 *  10FFFF     100001111111111111111  11110100 10001111 10111111 10111111   F4 8F BF BF # 4-byte UTF-8 encoding END
 *  ====================================================================================================================
 *
 *======================================================================================================================
 *  UTF-16 BIT DISTRIBUTION:
 *======================================================================================================================
 *  Code range	    Scalar value		UTF-16					Notes
 *  hexadecimal	    binary			binary
 *  --------------------------------------------------------------------------------------------------------------------
 *  000000-00FFFF   xxxxxxxxxxxxxxxx            xxxxxxxxxxxxxxxx                        All code points in the Basic
 *                  sixteen x                   sixteen x                               Multi-lingual Plane (BMP)
 *
 *  010000-10FFFF   000uuuuuxxxxxxxxxxxxxxxx    110110wwwwxxxxxx 110111xxxxxxxxxx       All code points in the
 *                  five u, sixteen x           four w, six x, ten x                    Suplementary Plane (non-BMP)
 *
 *                                where wwww = uuuuu - 1
 *=======================================================================================================================
 */

#define UTF8_NAME		"UTF-8"

#define	CHSET_M_STR		"M"
#define	CHSET_UTF8_STR		UTF8_NAME

#define	UTF8_1BYTE_MAX		(unsigned)(wint_t)ASCII_MAX
#define	UTF8_2BYTE_MAX		(unsigned)(wint_t)0x7FF
#define	UTF8_3BYTE_MAX		(unsigned)(wint_t)0xFFFF
#define	UTF8_4BYTE_MAX		(unsigned)(wint_t)0x10FFFF
#define U_BMP_MAX		(unsigned)(wint_t)0xFFFF
#define	U_SURROGATE_BEGIN	(unsigned)(wint_t)0xD800
#define	U_HIGH_SURROGATE_BEGIN	(unsigned)(wint_t)0xD800
#define	U_LOW_SURROGATE_BEGIN	(unsigned)(wint_t)0xDC00
#define	U_SURROGATE_END		(unsigned)(wint_t)0xDFFF

#define	UTF8_LEAD_2BYTEMASK	0x1F	/* extract out the x-s in 110xxxxx */
#define	UTF8_LEAD_3BYTEMASK	0x0F	/* extract out the x-s in 1110xxxx */
#define	UTF8_LEAD_4BYTEMASK	0x07	/* extract out the x-s in 11110xxx */
#define	UTF8_NONLEAD_BYTEMASK	0x3F	/* extract out the x-s in 10xxxxxx */

#define	UTF8_LEAD_2BYTE_PREFIX		0xC0	/* 110xxxxx where x is replaced with 0 */
#define	UTF8_LEAD_3BYTE_PREFIX		0xE0	/* 1110xxxx where x is replaced with 0 */
#define	UTF8_LEAD_4BYTE_PREFIX		0xF0	/* 11110xxx where x is replaced with 0 */
#define	UTF8_NONLEAD_BYTE_PREFIX	0x80	/* 10xxxxxx where x is replaced with 0 */

#define	UTF8_LEAD_2BYTE_BITLEN	5	/* number of bits extracted from the leading byte of a 2-byte UTF-8 encoding */
#define	UTF8_LEAD_3BYTE_BITLEN	4	/* number of bits extracted from the leading byte of a 3-byte UTF-8 encoding */
#define	UTF8_LEAD_4BYTE_BITLEN	3	/* number of bits extracted from the leading byte of a 4-byte UTF-8 encoding */
#define	UTF8_NONLEAD_BITLEN	6	/* number of bits extracted from the nonleading byte of a UTF-8 encoding */

#define	UTF_LINE_SEPARATOR	0x2028
#define	UTF_PARA_SEPARATOR	0x2029

#define	UTF8_SURROGATE_BYTELEN	3

#define GTM_MB_LEN_MAX		4	/* maximum bytes we support for multi-byte char */
#define GTM_MB_DISP_LEN_MAX	2	/* maximum number of display columns for a multi-byte char.
					 * all characters we know fit in a display width of 2 columns
					 */

/* This macro checks if a byte is a possible valid non-leading byte in a UTF-8 byte stream */
#define	UTF8_IS_VALID_TRAILING(x)	(((unsigned char)(x) & 0xc0) == 0x80)

/* This macro checks if a byte is a possible valid leading byte in a UTF-8 byte stream */
#define	UTF8_IS_VALID_LEADING(x)	(-1 != (int)utf8_followlen[(unsigned char)(x)])

/* boolean_t U_IS_SURROGATE_CODE(wint_t c)
 * 	Returns TRUE if the code point (c) of a character falls in the surrogate range.
 * 	Returns 0 otherwise.
 */
#define U_IS_SURROGATE_CODE(codepoint)					\
	(((unsigned)(codepoint) >= U_SURROGATE_BEGIN) 			\
	 	&& ((unsigned)(codepoint) <= U_SURROGATE_END))

/* boolean_t U_IS_SURROGATE_HIGH(wint_t c)
 * 	Returns TRUE if the code point (c) of a character is a leading (high) surrogate.
 * 	Returns 0 otherwise.
 */
#define U_IS_SURROGATE_HIGH(codepoint)					\
	(((unsigned)(codepoint) >= U_SURROGATE_BEGIN)			\
		&& ((unsigned)(codepoint) < U_LOW_SURROGATE_BEGIN))

/* boolean_t U_IS_SURROGATE_LOW(wint_t c)
 * 	Returns TRUE if the code point (c) of a character is a trailing (low) surrogate.
 * 	Returns 0 otherwise.
 */
#define U_IS_SURROGATE_LOW(codepoint)					\
	(((unsigned)(codepoint) >= U_LOW_SURROGATE_BEGIN)		\
		&& ((unsigned)(codepoint) <= U_SURROGATE_END))

/* If mbptr points to a valid 2-byte UTF-8 encoding this macro returns TRUE.
 * If not a valid 2-byte UTF-8 encoding, this macro returns FALSE, resets bytelen to 1.
 * Assumes "mbptr" is of type "uchar_ptr_t".
 */
#define	UTF8_VALID_2BYTE(mbptr, bytelen)						\
	((UTF8_IS_VALID_TRAILING(mbptr[1]))						\
		? TRUE									\
		: (bytelen = 1, FALSE))

/* boolean_t UTF8_NONCHAR_CODE_3BYTE(wint_t c)
 * boolean_t UTF8_NONCHAR_CODE_4BYTE(wint_t c)
 * 	Each of these macros returns TRUE if the code point (c) of a 3-byte or 4-byte wide-character is noncharacter, or
 * 	FALSE otherwise. Noncharacters are the code points that do not have valid character
 * 	assignment. This set includes:
 * 		U+FDD0 - U+FDEF (32 code points, all of which are 3-byte encoded)
 * 		All U+nFFFE and U+nFFFF, for each n from 0x0 to 0x10 (total of 34 code points,
 * 		of which U+FFFE and U+FFFF are 3-byte encoded and rest are 4-byte encoded)
 */
#define UTF8_NONCHAR_CODE_3BYTE(codepoint)								\
	((unsigned)(codepoint) >= 0xFDD0								\
	 	&& ((unsigned)(codepoint) <= 0xFDEF || ((unsigned)(codepoint) & 0xFFFE) == 0xFFFE))
#define UTF8_NONCHAR_CODE_4BYTE(codepoint)								\
	(((unsigned)(codepoint) & 0xFFFE) == 0xFFFE)

/* boolean_t UTF8_NONCHAR_CODEPOINT(wint_t c)
 * Returns TRUE if the codepoint (c) of ANY multi-byte character is a noncharacter or FALSE otherwise.
 * It assumes that UTF8_NONCHAR_CODE_3BYTE macro returns the correct values for any codepoint < UTF8_3BYTE_MAX
 * (including 1-byte and 2-byte codepoints).
 */
#define	UTF8_NONCHAR_CODEPOINT(codepoint)							\
	(((unsigned)(codepoint) <= UTF8_3BYTE_MAX) && (UTF8_NONCHAR_CODE_3BYTE(codepoint))	\
		||  UTF8_NONCHAR_CODE_4BYTE(codepoint))

/* boolean_t UTF8_NONCHAR_3BYTE(char* mbptr)
 * boolean_t UTF8_NONCHAR_4BYTE(char* mbptr)
 * 	Each of these macros returns TRUE if mbptr points to a noncharacter as described above
 * 	(or FALSE otherwise), except that the checks are performed on the UTF-8 byte stream
 * 	instead of the code points. Below are the equivalent byte patterns:
 * 	U+FDD0 - U+FDEF:
 * 		0xEF 0xB7 0x90  - 0xEF 0xB7 0xAF			(32 code points)
 * 	All U+nFFFE and U+nFFFF with the last two bytes having the byte patterns:
 * 		0xEF 0xBF 0xBE and 0xEF 0xBF 0xBF			(U+FFFE, U+FFFF)
 * 		0xF0 0x9F 0xBF 0xBE and 0xF0 0x9F 0xBF 0xBF		(U+1FFFE, U+1FFFF)
 * 		0xF0 0xAF 0xBF 0xBE and 0xF0 0xAF 0xBF 0xBF		(U+2FFFE, U+2FFFF)
 * 		....
 * 		....
 * 		0xF4 0x8F 0xBF 0xBE and 0xF4 0x8F 0xBF 0xBF		(U+10FFFE, U+10FFFF)
 */
#define UTF8_NONCHAR_3BYTE(mbptr)								\
	(mbptr[0] == 0xEF && ((mbptr[1] == 0xB7 && ((unsigned)(mbptr[2] - 0x90) < 32))		\
			|| ((mbptr[1] == 0xBF) && (mbptr[2] & 0xBE) == 0xBE)))
#define UTF8_NONCHAR_4BYTE(mbptr)								\
	(((mbptr[1] & 0x0F) == 0x0F) && (mbptr[2] == 0xBF) && (mbptr[3] & 0xBE) == 0xBE)

/* If mbptr points to a valid 3-byte UTF-8 encoding this macro returns TRUE.
 * If not a valid 3-byte UTF-8 encoding, this macro returns FALSE, resets bytelen to 1.
 * Assumes "mbptr" is of type "uchar_ptr_t".
 */
#define	UTF8_VALID_3BYTE(mbptr, bytelen)											\
	((UTF8_IS_VALID_TRAILING(mbptr[1]) && UTF8_IS_VALID_TRAILING(mbptr[2])							\
			&& ((mbptr[0] != 0xE0) || mbptr[1] >= 0xA0) /* ensure bytestream is above 3-byte UTF-8 BEGIN */ 	\
			&& ((mbptr[0] != 0xED) || mbptr[1] <= 0x9F) /* ensure bytestream is NOT a 3-byte UTF-8 surrogate */ 	\
	  		&& !UTF8_NONCHAR_3BYTE(mbptr))	/* ensure bytestream is NOT a 3-byte noncharacter */			\
		? TRUE														\
		: (bytelen = 1, FALSE))

/* If mbptr points to a valid 4-byte UTF-8 encoding this macro returns TRUE.
 * If not a valid 4-byte UTF-8 encoding, this macro returns FALSE, resets bytelen to 1.
 * Assumes "mbptr" is of type "uchar_ptr_t".
 */
#define	UTF8_VALID_4BYTE(mbptr, bytelen)											\
	((UTF8_IS_VALID_TRAILING(mbptr[1]) && UTF8_IS_VALID_TRAILING(mbptr[2]) && UTF8_IS_VALID_TRAILING(mbptr[3])		\
			&& ((mbptr[0] != 0xF0) || mbptr[1] >= 0x90) /* ensure bytestream is above 4-byte UTF-8 BEGIN */ 	\
			&& ((mbptr[0] != 0xF4) || mbptr[1] <= 0x8F) /* ensure bytestream is below 4-byte UTF-8 END */		\
	  		&& !UTF8_NONCHAR_4BYTE(mbptr))	/* ensure bytestream is NOT a 4-byte noncharacter */			\
		? TRUE														\
		: (bytelen = 1, FALSE))

/* If mbptr points to a valid 2-byte UTF-8 encoding this macro returns (mbptr + 2) and sets codepoint appropriately.
 * If not a valid 2-byte UTF-8 encoding, this macro returns (mbptr + 1), and sets codepoint to WEOF.
 * Assumes "mbptr" is of type "uchar_ptr_t" and that "codepoint" is already set to "mbptr[0]".
 */
#define	UTF8_MBTOWC_2BYTE(mbptr, codepoint)											\
	((UTF8_IS_VALID_TRAILING(mbptr[1]))											\
		? ((codepoint = ((codepoint & UTF8_LEAD_2BYTEMASK) << UTF8_NONLEAD_BITLEN) 					\
				| (mbptr[1] & UTF8_NONLEAD_BYTEMASK))								\
			, (mbptr + 2))												\
		: (codepoint = (wint_t)WEOF, (mbptr + 1)))

/* If mbptr points to a valid 3-byte UTF-8 encoding this macro returns (mbptr + 3) and sets codepoint appropriately.
 * If not a valid 3-byte UTF-8 encoding, this macro returns (mbptr + 1), and sets codepoint to WEOF.
 * Assumes "mbptr" is of type "uchar_ptr_t" and that "codepoint" is already set to "mbptr[0]".
 */
#define	UTF8_MBTOWC_3BYTE(mbptr, codepoint)											\
	((UTF8_IS_VALID_TRAILING(mbptr[1]) && UTF8_IS_VALID_TRAILING(mbptr[2])							\
			&& ((mbptr[0] != 0xE0) || mbptr[1] >= 0xA0) /* ensure bytestream is above 3-byte UTF-8 BEGIN */ 	\
			&& ((mbptr[0] != 0xED) || mbptr[1] <= 0x9F) /* ensure bytestream is NOT a 3-byte UTF-8 surrogate */ 	\
	  		&& !UTF8_NONCHAR_3BYTE(mbptr))	/* ensure bytestream is NOT a 3-byte noncharacter */			\
		? ((codepoint = ((((codepoint & UTF8_LEAD_3BYTEMASK)								\
				<< UTF8_NONLEAD_BITLEN) | (mbptr[1] & UTF8_NONLEAD_BYTEMASK))					\
				<< UTF8_NONLEAD_BITLEN) | (mbptr[2] & UTF8_NONLEAD_BYTEMASK))					\
			, (mbptr + 3))												\
		: (codepoint = (wint_t)WEOF, (mbptr + 1)))

/* If mbptr points to a valid 4-byte UTF-8 encoding this macro returns (mbptr + 4) and sets codepoint appropriately.
 * If not a valid 4-byte UTF-8 encoding, this macro returns (mbptr + 1), and sets codepoint to WEOF.
 * Assumes "mbptr" is of type "uchar_ptr_t" and that "codepoint" is already set to "mbptr[0]".
 */
#define	UTF8_MBTOWC_4BYTE(mbptr, codepoint)											\
	((UTF8_IS_VALID_TRAILING(mbptr[1]) && UTF8_IS_VALID_TRAILING(mbptr[2]) && UTF8_IS_VALID_TRAILING(mbptr[3])		\
			&& ((mbptr[0] != 0xF0) || mbptr[1] >= 0x90) /* ensure bytestream is above 4-byte UTF-8 BEGIN */ 	\
			&& ((mbptr[0] != 0xF4) || mbptr[1] <= 0x8F) /* ensure bytestream is below 4-byte UTF-8 END */		\
	  		&& !UTF8_NONCHAR_4BYTE(mbptr))	/* ensure bytestream is NOT a 4-byte noncharacter */			\
		? ((codepoint = ((((((codepoint & UTF8_LEAD_4BYTEMASK)								\
				<< UTF8_NONLEAD_BITLEN) | (mbptr[1] & UTF8_NONLEAD_BYTEMASK))					\
				<< UTF8_NONLEAD_BITLEN) | (mbptr[2] & UTF8_NONLEAD_BYTEMASK))					\
				<< UTF8_NONLEAD_BITLEN) | (mbptr[3] & UTF8_NONLEAD_BYTEMASK))					\
			, (mbptr + 4))												\
		: (codepoint = (wint_t)WEOF, (mbptr + 1)))

/* If mbptr points to a valid 2-byte UTF-8 encoding this macro returns (mbptr + 2).
 * If not a valid 2-byte UTF-8 encoding, this macro returns (mbptr + 1).
 * Assumes "mbptr" is of type "uchar_ptr_t".
 */
#define	UTF8_MBNEXT_2BYTE(mbptr)							\
	(UTF8_IS_VALID_TRAILING(mbptr[1])						\
		? (mbptr + 2)								\
		: (mbptr + 1))

/* If mbptr points to a valid 3-byte UTF-8 encoding this macro returns (mbptr + 3).
 * If not a valid 3-byte UTF-8 encoding, this macro returns (mbptr + 1).
 * Assumes "mbptr" is of type "uchar_ptr_t".
 */
#define	UTF8_MBNEXT_3BYTE(mbptr)												\
	((UTF8_IS_VALID_TRAILING(mbptr[1]) && UTF8_IS_VALID_TRAILING(mbptr[2])							\
			&& ((mbptr[0] != 0xE0) || mbptr[1] >= 0xA0) /* ensure bytestream is above 3-byte UTF-8 BEGIN */ 	\
			&& ((mbptr[0] != 0xED) || mbptr[1] <= 0x9F) /* ensure bytestream is NOT a 3-byte UTF-8 surrogate */ 	\
	  		&& !UTF8_NONCHAR_3BYTE(mbptr))	/* ensure bytestream is NOT a 3-byte noncharacter */			\
		? (mbptr + 3)													\
		: (mbptr + 1))

/* If mbptr points to a valid 4-byte UTF-8 encoding this macro returns (mbptr + 4).
 * If not a valid 4-byte UTF-8 encoding, this macro returns (mbptr + 1).
 * Assumes "mbptr" is of type "uchar_ptr_t".
 */
#define	UTF8_MBNEXT_4BYTE(mbptr)												\
	((UTF8_IS_VALID_TRAILING(mbptr[1]) && UTF8_IS_VALID_TRAILING(mbptr[2]) && UTF8_IS_VALID_TRAILING(mbptr[3])		\
			&& ((mbptr[0] != 0xF0) || mbptr[1] >= 0x90) /* ensure bytestream is above 4-byte UTF-8 BEGIN */ 	\
			&& ((mbptr[0] != 0xF4) || mbptr[1] <= 0x8F) /* ensure bytestream is below 4-byte UTF-8 END */		\
	  		&& !UTF8_NONCHAR_4BYTE(mbptr))	/* ensure bytestream is NOT a 4-byte noncharacter */			\
		? (mbptr + 4)													\
		: (mbptr + 1))

LITREF unsigned int	utf8_bytelen[];

/* boolean_t UTF8_VALID(char *ptr, char *ptrend, unsigned int bytelen)
 *	Inspects bytes of the multi-byte UTF-8 string "ptr" upto "ptrend" and Returns TRUE if the
 *	byte sequence beginning at s forms a welformed and complete UTF-8 character, or FALSE
 *	otherwise. Sets "bytelen" to the byte length of the UTF-8 character if returning TRUE and
 *	to 1 if returning FALSE. A well-formed UTF-8 codepoint that is either a surrogate (in the
 *	range D800 - DFFF) or a noncharacter is considered invalid.  This macro assumes that
 *	"ptrend" is at least "ptr+1" and does not do any checks on this.
 */
#define	UTF8_VALID(mbptr, ptrend, bytelen)										\
	((bytelen) = utf8_bytelen[((uchar_ptr_t)(mbptr))[0]],								\
		(((((uchar_ptr_t)(mbptr))[0]) <= ASCII_MAX) ? TRUE		/* ASCII. Do simplest check first. */	\
		: (((bytelen) == 1) ? FALSE					/* Invalid leading byte */		\
		: (((int4)(bytelen) > (int4)(((uchar_ptr_t)(ptrend)) - ((uchar_ptr_t)(mbptr))))				\
			? (bytelen = 1, FALSE)					/* Not enough length in input string */	\
		: ((bytelen) == 2 ? UTF8_VALID_2BYTE(((uchar_ptr_t)(mbptr)), (bytelen))					\
		: ((bytelen) == 3 ? UTF8_VALID_3BYTE(((uchar_ptr_t)(mbptr)), (bytelen))					\
		: /* bytelen == 4 */UTF8_VALID_4BYTE(((uchar_ptr_t)(mbptr)), (bytelen))					\
	))))))

/* boolean_t U_VALID_CODE(wint_t codepoint)
 * 	Returns
 * 		TRUE if the code point of a character is a valid Unicode code point
 * 		FALSE otherwise.
 * 	Invalid code points include:
 * 		All surrogate code points
 * 		All noncharacter code points
 * 		All code points greater than U+10FFFF
 */
#define U_VALID_CODE(codepoint)												\
	(((unsigned)(codepoint) <= UTF8_4BYTE_MAX)									\
	 	&& !U_IS_SURROGATE_CODE(codepoint)									\
	 	&& !UTF8_NONCHAR_CODEPOINT(codepoint))

LITREF signed int 	utf8_followlen[];

/* int UTF8_MBFOLLOW(char *s)
 *	Inspects only the first byte of a multi-byte (or even an incomplete) UTF-8 string
 *	pointed at s, and returns the numbers of bytes to follow in order to form a complete
 *	character. The possible return values by this macro are 0, 1, 2 or 3.
 *	If the byte stored at s does not form a legal first-byte of UTF-8 character,
 *	it returns -1.
 */
#define	UTF8_MBFOLLOW(mbptr)	(utf8_followlen[((uchar_ptr_t)(mbptr))[0]])

/* int UTF16BE_MBFOLLOW(char *mbptr, char *ptrend)
 *	Inspects up to two bytes of a multi-byte (or even an incomplete) UTF-16BE string
 *	pointed at mbptr, and returns the numbers of bytes to follow the byte at mbptr in order
 *	to form a complete UTF-16 character in BIG-ENDIAN format. The valid return values by
 *	this macro are 1 and 3.  If the number of bytes between [mbptr, ptrend) is less than 2,
 *	the macro returns -1.
 */
#define	UTF16BE_MBFOLLOW(mbptr, ptrend)								\
	((ptrend - mbptr >= 2) ? (UTF16BE_HIGH_SURROGATE(mbptr) ? 3 : 1) : -1)


/* int UTF16LE_MBFOLLOW(char *mbptr, char *ptrend)
 *	Inspects up to two bytes of a multi-byte (or even an incomplete) UTF-16LE string
 *	pointed at mbptr, and returns the numbers of bytes to follow the byte at mbptr in order
 *	to form a complete UTF-16 character in LITTLE-ENDIAN format. The valid return values by
 *	this macro are 1 and 3.  If the number of bytes between [mbptr, ptrend) is less than 2,
 *	the macro returns -1.
 */
#define	UTF16LE_MBFOLLOW(mbptr, ptrend)								\
	((ptrend - mbptr >= 2) ? (UTF16LE_HIGH_SURROGATE(mbptr) ? 3 : 1) : -1)

/* boolean_t UTF16BE_VALID(char *ptr, char *ptrend, unsigned int bytelen)
 *	Inspects 2 or 4 bytes of the UTF-16BE string "ptr" upto "ptrend" and Returns TRUE
 *	if the byte sequence beginning at ptr forms a welformed and complete UTF-16 character
 *	in big-endian format, or FALSE otherwise. This macro also sets "bytelen" to 2 (for
 *	BMP characters) or 4 (for surrogate pair).
 *	NOTES:
 *		"bytelen" is always set irrespective of the validity of the code point (eg.
 *		it can be set to 4 for surrogate pair for which the macro returns FALSE
 *		because its code point is not valid (non-character).
 *
 *		"ptrend" is asummed to be at least "ptr+2"
 */
#define	UTF16BE_VALID(mbptr, ptrend, bytelen)							\
	(UTF16BE_HIGH_SURROGATE(mbptr) /* compute the code point first */			\
	 	? (((ptrend - mbptr) >= 4 && UTF16BE_LOW_SURROGATE(mbptr + 2))			\
			? ((UTF16BE_LOAD_SURROGATE(mbptr, bytelen), U_VALID_CODE(bytelen)) 	\
				? (bytelen = 4, TRUE) : (bytelen = 4, FALSE))			\
			: (bytelen = 2, FALSE))							\
		: (((bytelen = UTF16BE_GET_UNIT(mbptr)), U_VALID_CODE(bytelen))			\
			? (bytelen = 2, TRUE) : (bytelen = 2, FALSE)))

/* boolean_t UTF16LE_VALID(char *ptr, char *ptrend, unsigned int bytelen)
 *	Inspects 2 or 4 bytes of the UTF-16BE string "ptr" upto "ptrend" and Returns TRUE
 *	if the byte sequence beginning at ptr forms a welformed and complete UTF-16 character
 *	in little-endian format, or FALSE otherwise. This macro also sets "bytelen" to 2 (for
 *	BMP characters) or 4 (for surrogate pair).
 *	NOTES:
 *		"bytelen" is always set irrespective of the validity of the code point (eg.
 *		it can be set to 4 for surrogate pair for which the macro returns FALSE
 *		because its code point is not valid (non-character).
 *
 *		"ptrend" is asummed to be at least "ptr+2"
 */
#define	UTF16LE_VALID(mbptr, ptrend, bytelen)							\
	(UTF16LE_HIGH_SURROGATE(mbptr) /* compute the code point first */			\
	 	? (((ptrend - mbptr) >= 4 && UTF16LE_LOW_SURROGATE(mbptr + 2))			\
			? ((UTF16LE_LOAD_SURROGATE(mbptr, bytelen), U_VALID_CODE(bytelen)) 	\
				? (bytelen = 4, TRUE) : (bytelen = 4, FALSE))			\
			: (bytelen = 2, FALSE))							\
		: (((bytelen = UTF16LE_GET_UNIT(mbptr)), U_VALID_CODE(bytelen))			\
			? (bytelen = 2, TRUE) : (bytelen = 2, FALSE)))

/* unsigned char *UTF8_MBTOWC(char *mbptr, char *ptrend, wint_t codepoint)
 *	Inspects bytes of the UTF-8 string upto ptrend and sets "codepoint" to the code point of
 *	the next character in the string. If the bytes starting from mbptr do not form a complete
 *	wellformed UTF-8 character, it sets "codepoint" to WEOF. Returns (mbptr+len) where "len"
 *	is the byte length of the UTF-8 character found.  If "codepoint" is set to WEOF, the return
 *	value is (mbptr+1).
 */
#define	UTF8_MBTOWC(mbptr, ptrend, codepoint)										\
	((codepoint) = (wint_t)(((uchar_ptr_t)(mbptr))[0]), 								\
		(((codepoint) <= ASCII_MAX) ? ((uchar_ptr_t)mbptr + 1)		/* ASCII. Do simplest check first. */	\
		: ((utf8_bytelen[(codepoint)] == 1)				/* Invalid leading byte */		\
			? ((codepoint) = (wint_t)WEOF, ((uchar_ptr_t)mbptr + 1))					\
		: (((int4)utf8_bytelen[(codepoint)]				/* Not enough length in input string */	\
				> (int4)(((uchar_ptr_t)(ptrend)) - ((uchar_ptr_t)(mbptr))))				\
			? ((codepoint) = (wint_t)WEOF, ((uchar_ptr_t)mbptr + 1))					\
		: (utf8_bytelen[(codepoint)] == 2 ? UTF8_MBTOWC_2BYTE(((uchar_ptr_t)(mbptr)), (codepoint))		\
		: (utf8_bytelen[(codepoint)] == 3 ? UTF8_MBTOWC_3BYTE(((uchar_ptr_t)(mbptr)), (codepoint))		\
		: /* utf8_bytelen[codepoint] == 4 */UTF8_MBTOWC_4BYTE(((uchar_ptr_t)(mbptr)), (codepoint))		\
	))))))

/* unsigned char* UTF8_MBNEXT(char *ptr, char *ptrend)
 *	Assuming that the string pointed at ptr is wellformed, it inspects bytes upto ptrend
 *	and advances the pointer by the number of bytes used by the character pointed at ptr.
 *	It returns the pointer to the beginning of the following character. If the bytes
 *	starting from s do not form a welformed character within the limits defined
 *	by ptrend, it returns the pointer to the next byte (i.e. s+1).
 */
#define	UTF8_MBNEXT(mbptr, ptrend)												\
	(((((uchar_ptr_t)(mbptr))[0]) <= ASCII_MAX) ? ((uchar_ptr_t)mbptr + 1)		/* ASCII. Do simplest check first. */	\
		: ((utf8_bytelen[(((uchar_ptr_t)(mbptr))[0])] == 1)			/* Invalid leading byte */		\
			? ((uchar_ptr_t)mbptr + 1)										\
		: (((int4)utf8_bytelen[(((uchar_ptr_t)(mbptr))[0])] > (int4)((uchar_ptr_t)ptrend - (uchar_ptr_t)mbptr))		\
			? ((uchar_ptr_t)mbptr + 1)					/* Not enough length in input string */	\
		: (utf8_bytelen[(((uchar_ptr_t)(mbptr))[0])] == 2 ? UTF8_MBNEXT_2BYTE(((uchar_ptr_t)(mbptr)))			\
		: (utf8_bytelen[(((uchar_ptr_t)(mbptr))[0])] == 3 ? UTF8_MBNEXT_3BYTE(((uchar_ptr_t)(mbptr)))			\
		: /* utf8_bytelen[(((uchar_ptr_t)mbptr)[0])] == 4 */UTF8_MBNEXT_4BYTE(((uchar_ptr_t)(mbptr)))			\
	)))))


/* unsigned char* UTF8_WCTOMB(wint_t c, char *s)
 *	Converts the code point of a character (c) to a sequence of bytes and stores
 *	the result (of 1 to 4 bytes long) at the beginning of the character array pointed
 *	to by s. It returns the pointer advanced by the number of bytes required for c.
 *	For invalid code points no conversion is done and and the macro returns s.
 */
#define	UTF8_WCTOMB(codepoint, mbptr)												\
	(((unsigned)(codepoint) <= UTF8_1BYTE_MAX)			/* 1-byte UTF-8 encoding */				\
		? (*((uchar_ptr_t)mbptr) = (unsigned char)(codepoint), ((uchar_ptr_t)mbptr) + 1)				\
	: (((unsigned)(codepoint) <= UTF8_2BYTE_MAX)			/* 2-byte UTF-8 encoding */				\
		? (*(((uchar_ptr_t)mbptr) + 1)											\
			= (unsigned char)(((codepoint) & UTF8_NONLEAD_BYTEMASK) | UTF8_NONLEAD_BYTE_PREFIX),			\
			*((uchar_ptr_t)mbptr) = (unsigned char)(((codepoint) >> UTF8_NONLEAD_BITLEN) | UTF8_LEAD_2BYTE_PREFIX),	\
			((uchar_ptr_t)mbptr) + 2)										\
	: (((unsigned)(codepoint) <= UTF8_3BYTE_MAX)			/* 3-byte UTF-8 encoding */				\
		? ((U_IS_SURROGATE_CODE(codepoint) || UTF8_NONCHAR_CODE_3BYTE(codepoint))					\
			? ((uchar_ptr_t)mbptr)			/* Surrogate or noncharacter (3-byte case) */			\
			: (*(((uchar_ptr_t)mbptr) + 2)		/* Non-surrogate 3-byte case */					\
				= (unsigned char)(((codepoint) & UTF8_NONLEAD_BYTEMASK) | UTF8_NONLEAD_BYTE_PREFIX),		\
				*(((uchar_ptr_t)mbptr) + 1)									\
					= (unsigned char)((((codepoint) >> UTF8_NONLEAD_BITLEN) & UTF8_NONLEAD_BYTEMASK)	\
									| UTF8_NONLEAD_BYTE_PREFIX),				\
				*((uchar_ptr_t)mbptr) = (unsigned char)(((codepoint) >> (2 * UTF8_NONLEAD_BITLEN))		\
									| UTF8_LEAD_3BYTE_PREFIX),				\
				((uchar_ptr_t)mbptr) + 3))									\
	: ((((unsigned)(codepoint) <= UTF8_4BYTE_MAX) && !UTF8_NONCHAR_CODE_4BYTE(codepoint))	/* 4-byte UTF-8 encoding */	\
		? (*(((uchar_ptr_t)mbptr) + 3)											\
			= (unsigned char)(((codepoint) & UTF8_NONLEAD_BYTEMASK) | UTF8_NONLEAD_BYTE_PREFIX),			\
			*(((uchar_ptr_t)mbptr) + 2)										\
				= (unsigned char)((((codepoint) >> UTF8_NONLEAD_BITLEN) & UTF8_NONLEAD_BYTEMASK)		\
									| UTF8_NONLEAD_BYTE_PREFIX),				\
			*(((uchar_ptr_t)mbptr) + 1)										\
				= (unsigned char)((((codepoint) >> (2 * UTF8_NONLEAD_BITLEN)) & UTF8_NONLEAD_BYTEMASK)		\
									| UTF8_NONLEAD_BYTE_PREFIX),				\
			*((uchar_ptr_t)mbptr) = (unsigned char)(((codepoint) >> (3 * UTF8_NONLEAD_BITLEN))			\
								| UTF8_LEAD_4BYTE_PREFIX),					\
			((uchar_ptr_t)mbptr) + 4)										\
	: ((uchar_ptr_t)mbptr)))))

/* boolean_t UTF8_SURROGATE(char* s, char *ptrend)
 *	Inspects bytes of the multi-byte UTF-8 string upto ptrend and Returns TRUE if the
 *	byte sequence beginning at s forms a welformed UTF-8 character and an
 *	isolated surrogate character (either lower surrogate or upper surrogate).
 *	It returns FALSE, otherwise.
 */
#define	UTF8_SURROGATE(mbptr, ptrend)											\
	(((UTF8_SURROGATE_BYTELEN			/* maxlen should be at least 3-bytes */				\
			<= ((int4)((uchar_ptr_t)ptrend - (uchar_ptr_t)mbptr))) 						\
		&& (((uchar_ptr_t)mbptr)[0] == 0xED)	/* leading byte should be 0xED for surrogate UTF-8 */		\
		&& (((uchar_ptr_t)mbptr)[1] >= 0xA0)	/* first non-leading byte should be at least 0xA0 */		\
		&& (((uchar_ptr_t)mbptr)[1] <= 0xBF)	/* first non-leading byte should be at most  0xBF */		\
		&& (UTF8_IS_VALID_TRAILING(((uchar_ptr_t)mbptr)[2])))	/* second non-leading byte should be valid */	\
	? TRUE : FALSE)

/* void UTF8_LEADING_BYTE(char* mbptr, char* baseptr, char* leadptr)
 * 	Sets leadptr to point to the leading byte of the UTF-8 character containing the byte
 * 	pointed by mbptr. If the byte pointed by mbptr is not part of a valid UTF-8 character,
 * 	this macro sets leadptr to mbptr.
 * 	NOTE: mbptr and leadptr must not be the same variable.
 */
#define UTF8_LEADING_BYTE(mbptr, baseptr, leadptr)						\
{												\
	leadptr = mbptr;									\
	while (leadptr >= baseptr && UTF8_IS_VALID_TRAILING(*(uchar_ptr_t)leadptr))		\
		--leadptr;									\
	if (leadptr < baseptr || !UTF8_IS_VALID_LEADING(*(uchar_ptr_t)leadptr) || 		\
			(mbptr - leadptr) > utf8_followlen[*(uchar_ptr_t)leadptr])		\
		leadptr = mbptr;								\
}

/* Macros to return the UTF-16 (16-bit) code units from a given code point in the supplementary plane.
 * Note: these macros must be called only for the supplementary code points (> U_BMP_MAX) that are <= UTF8_4BYTE_MAX */
#define UTF16_HIGH_SURROGATE(codepoint)									\
	(U_HIGH_SURROGATE_BEGIN | ((((codepoint) >> 16) - 1) << 6) | (((codepoint) >> 10) & 0x3F))
#define UTF16_LOW_SURROGATE(codepoint) 									\
	(U_LOW_SURROGATE_BEGIN | ((codepoint) & 0x3FF))

/* Composes a surrogate pair and returns the code point in the supplementary plane */
#define UTF16_COMPOSE_SURROGATES(high, low)								\
	((((((high) >> 6) & 0xF) + 1) << 16) | (((high) & 0x3F) << 10) | ((low) & 0x3FF))

/* Macros to convert a UTF-16 (16-bit) code unit into a 2-byte sequence in the appropriate endianness.
 * The codeunits passed must be less than or equal to U_BMP_MAX */
#define UTF16BE_STORE_UNIT(mbptr, codeunit)								\
	((((uchar_ptr_t)mbptr)[0] = ((codeunit) >> 8)), (((uchar_ptr_t)mbptr)[1] = ((codeunit) & 0x00FF)))
#define UTF16LE_STORE_UNIT(mbptr, codeunit)								\
	((((uchar_ptr_t)mbptr)[1] = ((codeunit) >> 8)), (((uchar_ptr_t)mbptr)[0] = ((codeunit) & 0x00FF)))

/* macros to return a single UTF-16 (16-bit) codeunit given a 2-byte sequence */
#define UTF16BE_GET_UNIT(mbptr)										\
	((((uchar_ptr_t)mbptr)[0] << 8) | ((uchar_ptr_t)mbptr)[1])
#define UTF16LE_GET_UNIT(mbptr)										\
	((((uchar_ptr_t)mbptr)[1] << 8) | ((uchar_ptr_t)mbptr)[0])

/* macros to load UTF-16 surrogate codeunit pairs and return the code point in the supplementary plane.
 * Note: mbptr must point to a valid 4-byte sequence of high and low surrogates */
#define UTF16BE_LOAD_SURROGATE(mbptr, codepoint)							\
	 (codepoint = UTF16BE_GET_UNIT(mbptr), 								\
	  	codepoint = UTF16_COMPOSE_SURROGATES(codepoint, UTF16BE_GET_UNIT(mbptr+2)))
#define UTF16LE_LOAD_SURROGATE(mbptr, codepoint)							\
	 (codepoint = UTF16LE_GET_UNIT(mbptr), 								\
	  	codepoint = UTF16_COMPOSE_SURROGATES(codepoint, UTF16LE_GET_UNIT(mbptr+2)))

/* char* UTF16BE_WCTOMB(wint_t codepoint, char *mbptr)
 *	Converts the code point of a character (codepoint) in to big-endian UTF-16 bytes
 *	and stores the result (of 2 or 4 bytes long) at the beginning of the character
 *	array pointed to by mbptr. It returns the pointer advanced by the number of bytes
 *	required for codepoint. For invalid code points, no conversion is done and and
 *	the macro returns mbptr.
 */
#define	UTF16BE_WCTOMB(codepoint, mbptr)									\
	(U_VALID_CODE(codepoint)										\
	 	? ((codepoint) <= U_BMP_MAX 									\
			? (UTF16BE_STORE_UNIT(mbptr, (codepoint)), mbptr + 2) 	/* code points in BMP */	\
			: (UTF16BE_STORE_UNIT(mbptr, UTF16_HIGH_SURROGATE(codepoint)), /* supplementary plane */\
			   UTF16BE_STORE_UNIT(mbptr + 2, UTF16_LOW_SURROGATE(codepoint)), mbptr + 4))		\
		: mbptr)

/* char* UTF16LE_WCTOMB(wint_t codepoint, char *mbptr)
 *	Converts the code point of a character (codepoint) in to little-endian UTF-16 bytes
 *	and stores the result (of 2 or 4 bytes long) at the beginning of the character
 *	array pointed to by mbptr. It returns the pointer advanced by the number of bytes
 *	required for codepoint. For invalid code points, no conversion is done and and
 *	the macro returns mbptr.
 */
#define	UTF16LE_WCTOMB(codepoint, mbptr)									\
	(U_VALID_CODE(codepoint)										\
	 	? ((codepoint) <= U_BMP_MAX		/* 16-bit characters */					\
			? (UTF16LE_STORE_UNIT(mbptr, (codepoint)), mbptr + 2) 	/* code points in BMP */	\
			: (UTF16LE_STORE_UNIT(mbptr, UTF16_HIGH_SURROGATE(codepoint)), /* supplementary plane */\
			   UTF16LE_STORE_UNIT(mbptr + 2, UTF16_LOW_SURROGATE(codepoint)), mbptr + 4))		\
		: mbptr)

/* char *UTF16BE_MBTOWC(char *mbptr, char *ptrend, wint_t codepoint)
 *	Inspects 2 bytes (or 4 bytes if surrogates) of the UTF-16 string in big-endian and
 *	sets "codepoint" to the code point of the next character in the string. Returns
 *	(mbptr + len) where "len" is the byte length of the UTF-16BE character found. If
 *	the bytes starting from mbptr do not form a complete welformed UTF-16BE character,
 *	it sets codepoint to WEOF and return mbptr.
 */
#define	UTF16BE_MBTOWC(mbptr, ptrend, codepoint)						\
	((UTF16BE_HIGH_SURROGATE(mbptr) /* compute the code point first */			\
	 	? (((ptrend - mbptr) >= 4 && UTF16BE_LOW_SURROGATE(mbptr + 2))			\
			? UTF16BE_LOAD_SURROGATE(mbptr, codepoint) : (codepoint = WEOF))	\
		: (codepoint = UTF16BE_GET_UNIT(mbptr))),					\
	(U_VALID_CODE(codepoint)	/* validate the code point */				\
		? ((codepoint) <= U_BMP_MAX ? (mbptr + 2) : (mbptr + 4))			\
		: (((codepoint) = WEOF), mbptr)))

/* char *UTF16LE_MBTOWC(char *mbptr, char *ptrend, wint_t codepoint)
 *	Inspects 2 bytes (or 4 bytes if surrogates) of the UTF-16 string in little-endian and
 *	sets "codepoint" to the code point of the next character in the string. Returns
 *	(mbptr + len) where "len" is the byte length of the UTF-16BE character found. If
 *	the bytes starting from mbptr do not form a complete welformed UTF-16LE character,
 *	it sets codepoint to WEOF and return mbptr.
 */
#define	UTF16LE_MBTOWC(mbptr, ptrend, codepoint)						\
	((UTF16LE_HIGH_SURROGATE(mbptr) /* compute the code point first */			\
	 	? (((ptrend - mbptr) >= 4 && UTF16LE_LOW_SURROGATE(mbptr + 2))			\
			? UTF16LE_LOAD_SURROGATE(mbptr, codepoint) : (codepoint = WEOF))	\
		: (codepoint = UTF16LE_GET_UNIT(mbptr))),					\
	(U_VALID_CODE(codepoint)	/* validate the code point */				\
		? ((codepoint) <= U_BMP_MAX ? (mbptr + 2) : (mbptr + 4))			\
		: (((codepoint) = WEOF), mbptr)))

/* boolean_t UTF16BE_HIGH_SURROGATE(char* mbptr)
 * 	Inspects at most 2 bytes in the UTF-16 string and Returns TRUE if the byte sequence
 * 	beginning at mbptr forms a welformed UTF-16BE high surrogate character (U+D800 - U+DBFF)
 * 	and FALSE otherwise.
 */
#define UTF16BE_HIGH_SURROGATE(mbptr)								\
	(U_IS_SURROGATE_HIGH(UTF16BE_GET_UNIT(mbptr)))

/* boolean_t UTF16LE_HIGH_SURROGATE(char* mbptr)
 * 	Inspects at most 2 bytes in the UTF-16 string and Returns TRUE if the byte sequence
 * 	beginning at mbptr forms a welformed UTF-16LE high surrogate character (U+D800 - U+DBFF)
 * 	and FALSE otherwise.
 */
#define UTF16LE_HIGH_SURROGATE(mbptr)								\
	(U_IS_SURROGATE_HIGH(UTF16LE_GET_UNIT(mbptr)))

/* boolean_t UTF16BE_LOW_SURROGATE(char* mbptr)
 * 	Inspects at most 2 bytes in the UTF-16 string and Returns TRUE if the byte sequence
 * 	beginning at mbptr forms a welformed UTF-16BE low surrogate character (U+DC00 - U+DFFF)
 * 	and FALSE otherwise.
 */
#define UTF16BE_LOW_SURROGATE(mbptr)								\
	(U_IS_SURROGATE_LOW(UTF16BE_GET_UNIT(mbptr)))

/* boolean_t UTF16LE_LOW_SURROGATE(char* mbptr)
 * 	Inspects at most 2 bytes in the UTF-16 string and Returns TRUE if the byte sequence
 * 	beginning at mbptr forms a welformed UTF-16LE low surrogate character (U+DC00 - U+DFFF)
 * 	and FALSE otherwise.
 */
#define UTF16LE_LOW_SURROGATE(mbptr)								\
	(U_IS_SURROGATE_LOW(UTF16LE_GET_UNIT(mbptr)))

/* The following macros provide the character classification for Unicode characters given their code points */
#define U_ISLOWER(c)	u_islower(c)
#define U_ISUPPER(c)	u_isupper(c)
#define U_ISALPHA(c)	u_isalpha(c)
#define U_ISCNTRL(c)	u_iscntrl(c)
#define U_ISDIGIT(c)	u_isdigit(c)
#define U_ISPUNCT(c)	u_ispunct(c)
#define U_ISSPACE(c)	u_isspace(c)
#define U_ISBLANK(c)	u_isblank(c)
#define U_ISGRAPH(c)	u_isgraph(c)
#define U_ISPRINT(c)	GTM_U_ISPRINT(c) /* see macro definition for why redirection needed */
#define U_ISTITLE(c)	u_istitle(c)
#define U_CHARTYPE(c)	u_charType(c)

/* uint4	CTYPEMASK(wint_t c)
 *
 * This macro assumes that "c" is a valid unicode codepoint.
 *
 * Returns a patcode from a code point (wide character wint_t) paralleling the way ICU library functions classify codepoints.
 * 	u_isalpha (for A)
 * 	u_isdigit (for N)
 * 	u_ispunct (for P)
 * 	u_iscntrl (for C)
 * But with the following adjustments.
 *	1) If $ZPATNUMERIC is not "UTF-8", non-ASCII decimal digits are classified as A.
 *	2) Non-decimal digits (Nl and No) are classified as A. Note: u_isdigit only matches decimal digits.
 *	3) Anything left is classified via u_isprint into either P or C. Note: u_isprint only matches non-control characters.
 * Note that the ISV $ZPATN[UMERIC] dictates how the pattern class N used in the pattern match operator is interpreted.
 * If $ZPATNUMERIC is "UTF-8", the pattern class N matches any decimal numeric character as defined by the Unicode standard.
 * If $ZPATNUMERIC is "M", GT.M restricts the pattern class N to match only ASCII digits 0-9 (i.e. ASCII 48-57).
 * The variable "utf8_patnumeric" is TRUE if $ZPATNUMERIC is "UTF-8".
 *
 * The above rules result in the following mapping
 *      --------------------------------------------------
 *      Unicode general category       GT.M patcode class
 *      --------------------------------------------------
 *	L* (all letters)	    -> A
 *	M* (all marks)		    -> P
 *	Nd (decimal numbers)	    -> N (if decimal digit is ASCII or $ZPATNUMERIC is "UTF-8", otherwise -> A)
 *	Nl (letter numbers)	    -> A (examples of Nl are Roman numerals)
 *	No (other numbers)	    -> A (examples of No are fractions)
 *	P* (all punctuation)	    -> P
 *	S* (all symbols)	    -> P
 *	Zs (spaces)		    -> P
 *	Zl (line separators)	    -> C
 *	Zp (paragraph separators)   -> C
 *	C* (all control codepoints) -> C
 *
 * For a description of the Unicode general categories see http://unicode.org/versions/Unicode4.0.0/ch04.pdf (section 4.5)
 *
 * E = A + P + N + C and the classifications A, P, N, and C are mutually exclusive.
 *
 * This means that PATM_UTF8_NONBASIC does not currently have any codepoints mapped to it.
 * It is being retained in case it is needed in the future.
 */

/* our mask to map non-decimal digits into the PATM_A  */
#define GTM_NA_MASK (U_GC_NL_MASK | U_GC_NO_MASK)

#define	CTYPEMASK(c)														\
	(U_ISALPHA(c) ?						/* alphabet */							\
		(U_ISLOWER(c) ? PATM_L				/* lower-case */						\
			: (U_ISUPPER(c) ? PATM_U		/* upper-case */						\
				: PATM_UTF8_ALPHABET))		/* unicode alphabet that is neither lower nor upper case */	\
		: (U_ISDIGIT(c)					/* ascii or non-ascii decimal digit */				\
			? ((utf8_patnumeric || IS_ASCII(c))	/* check $ZPATNUMERIC setting */				\
				? PATM_N			/* Ascii digit OR $ZPATNUMERIC set to "UTF-8" */		\
				: PATM_UTF8_ALPHABET)		/* $ZPATNUMERIC set to "M" and non-ascii decimal digit */	\
			: ((U_MASK(U_CHARTYPE(c)) & GTM_NA_MASK)/* put non-decimal digits in  */				\
				? PATM_UTF8_ALPHABET 		/* PATM_UTF8_ALPHABET */					\
				: (U_ISPUNCT(c) ? PATM_P	/* punctuation */						\
					:(U_ISCNTRL(c) ? PATM_C /* control */							\
								/* unicode character that is not part of any basic class */	\
						:(U_ISPRINT(c) 		/* if printable  */					\
							? PATM_P	/* punctuation */					\
							: PATM_C))))))	/* otherwise, control */

/* uint4	TYPEMASK(char *ptr, char *ptrend, char *ptrnext, wint_t codepoint)
 * Inspects bytes of a character (in UTF-8 format) starting at "ptr" upto "ptrend", and returns its patcode.
 * This macro is a replacment to the existing typemask[] table that works in both UTF-8 and non-UTF8 mode.
 * This macro should only be used by the compiler. This macro assumes that "gtm_utf8_mode" is TRUE.
 * The parameter "codepoint" is set to the codepoint which is the same thing that gets returned by the macro.
 */
#define	TYPEMASK(ptr, ptrend, ptrnext, codepoint)						\
	(IS_ASCII(*(ptr))									\
		? ((ptrnext = ptr + 1), (codepoint) = *(ptr), typemask[*(ptr)])			\
		: (ptrnext = UTF8_MBTOWC(ptr, ptrend, codepoint), CTYPEMASK(codepoint)))

/* uint4	PATTERN_TYPEMASK(char *ptr, char *ptrend, char *ptrnext, wint_t codepoint)
 * Inspects bytes of a character (in UTF-8 format) starting at "ptr" upto "ptrend", and returns its patcode.
 * This macro is a replacment to the existing pattern_typemask[] table that works in both UTF-8 and non-UTF8 mode.
 * This macro should only be used by the runtime. This macro assumes that "gtm_utf8_mode" is TRUE.
 * The parameter "codepoint" is set to the codepoint if it is a multi-byte UTF8 character.
 */
#define	PATTERN_TYPEMASK(ptr, ptrend, ptrnext, codepoint)					\
	(IS_ASCII(*(ptr))									\
		? ((ptrnext = ptr + 1), (codepoint) = *(ptr), pattern_typemask[*(ptr)])		\
		: (ptrnext = UTF8_MBTOWC(ptr, ptrend, codepoint), CTYPEMASK(codepoint)))


/* Returns the display column width of a character given its code point. This macro
 * returns -1 for control characters and 0 for non-spacing (combining) characters
 */
#define UTF8_WCWIDTH(c)	gtm_wcwidth((wint_t)(c))

/* The following macro is same as UTF8_WCWIDTH except that it returns 0 for unprintable valid characters as well.
 * It is primarily used by the IO code.
 */
#ifdef UNICODE_SUPPORTED
#define GTM_IO_WCWIDTH(CHAR,RET)		\
	if (utf8_active)			\
	{					\
		RET = UTF8_WCWIDTH(CHAR);	\
		RET = (0 > RET ? 0 : RET);	\
	} else					\
		RET = 1
#else
#define GTM_IO_WCWIDTH(CHAR,RET)	RET = 1
#endif

/* Offsets for use with u32_line_term[] */
#define U32_LT_LF	0
#define U32_LT_CR	1
#define U32_LT_NL	2
#define U32_LT_FF	3
#define U32_LT_LS	4
#define U32_LT_PS	5
#define U32_LT_LAST	5	/* not counting null sentinel */
#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
int	trim_U16_line_term(UChar *buffer, int len);
#endif

/* There could be integral promotion/sign extension issues if short, int (or an integral type) is used for comparison. Avoid
 * such issues by definining BOM as a string
 */
#define UTF16BE_BOM		"\xFE\xFF"	/* Big Endian BYTE ORDER MARKER */
#define UTF16BE_BOM_LEN		STR_LIT_LEN(UTF16BE_BOM)

#define UTF16LE_BOM		"\xFF\xFE"	/* Little Endian BYTE ORDER MARKER */
#define UTF16LE_BOM_LEN		STR_LIT_LEN(UTF16LE_BOM)

#define UTF8_BOM		"\xEF\xBB\xBF"	/* No relevance to endian-ness, a UTF8 MARKER similar to UTF16_BOM */
#define UTF8_BOM_LEN		STR_LIT_LEN(UTF8_BOM)

#define UTF32BE_BOM		"\x00\x00\xFE\xFF"	/* Big Endian BYTE ORDER MARKER */
#define UTF32BE_BOM_LEN		STR_LIT_LEN(UTF32BE_BOM)

#define UTF32LE_BOM		"\xFF\xFE\x00\x00"	/* Little Endian BYTE ORDER MARKER */
#define UTF32LE_BOM_LEN		STR_LIT_LEN(UTF32LE_BOM)

#define	BOM_CODEPOINT		0xFEFF

#define	UTF8_BADCHAR(len, str, strtop, chset_len, chset)								\
	utf8_badchar((len), (unsigned char *)(str), (unsigned char *)(strtop), (chset_len), (unsigned char *)(chset))

#define	UTF8_BADCHAR_STX(len, str, strtop, chset_len, chset)								\
	utf8_badchar_stx((len), (unsigned char *)(str), (unsigned char *)(strtop), (chset_len), (unsigned char *)(chset))

#define	UTF8_LEN_STRICT(ptr, len)			\
	utf8_len_strict((unsigned char *)(ptr), (len))

/* This macro is needed to to ensure all Unicode line terminators are considered non-printable. As of this
 * writing, ICU's u_isprint returns TRUE for LS/PS (Line/Paragraph separator; codepoints 0x2028, 0x2029)
 * and this causes problems in extracting and loading data which contains these codepoints (in UTF8 mode).
 * Ideally, one should go through all the line terminators in u32_line_term[] array and check them.
 * But since this routine is performance intensive (called from mupip extract which can take hours for
 * huge databases) we avoid a loop and just check for LS/PS which we know dont work right with "u_isprint"
 */
#define	GTM_U_ISPRINT(code)												\
	((((UChar32)UTF_LINE_SEPARATOR == (UChar32)(code)) || ((UChar32)UTF_PARA_SEPARATOR == (UChar32)(code)))		\
		? FALSE													\
		: u_isprint(code))

GBLREF		boolean_t       utf8_patnumeric;
int		utf8_len(mstr* str);
int		utf8_len_stx(mstr* str);
int		utf8_len_strict(unsigned char* ptr, int len);
int		gtm_wcwidth(wint_t code);
int		gtm_wcswidth(unsigned char* ptr, int len, boolean_t strict, int nonprintwidth);
void		utf8_badchar(int len, unsigned char* str, unsigned char *strtop, int chset_len, unsigned char* chset);
void		utf8_badchar_stx(int len, unsigned char* str, unsigned char *strtop, int chset_len, unsigned char* chset);
unsigned char	*gtm_utf8_trim_invalid_tail(unsigned char *str, int len);

/* To prevent GTMSECSHR from pulling in the function "gtmwcswidth" (used in util_output.c) and in turn the entire Unicode
 * codebase, we define a function-pointer variable and initialize it at startup to NULL only in GTMSECSHR and not-null
 * in all the other executables.
 */
typedef	int	(*gtm_wcswidth_fnptr_t)(unsigned char* ptr, int len, boolean_t strict, int nonprintwidth);

GBLREF	gtm_wcswidth_fnptr_t	gtm_wcswidth_fnptr;	/* see comment in gtm_utf8.h about this typedef */

#endif /* GTM_UTF8_H */
