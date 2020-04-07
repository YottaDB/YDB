/****************************************************************
 *								*
 * Copyright (c) 2006-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 * 								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidaries.	*
 * All rights reserved.						*
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
#include "have_crit.h"
#include "mvalconv.h"
#include "gtm_caseconv.h"
#include "gtm_ctype.h"
#include "gtm_stdlib.h"


GBLREF	boolean_t	badchar_inhibit;
GBLREF	boolean_t	gtm_utf8_mode;
GBLREF	spdesc 		stringpool;
GBLREF	casemap_t	casemaps[];

error_def(ERR_BADCASECODE);
error_def(ERR_BADCHSET);
error_def(ERR_ICUERROR);
error_def(ERR_MAXSTRLEN);
error_def(ERR_INVFCN);
error_def(ERR_TEXT);

#define D_STR "DEC"
#define H_STR "HEX"
#define DH_TYPE_LEN 3
#define MAX_POS "18446744073709551615"
#define MAX_NEG "9223372036854775808"
#define DEC_RANGE_ERR_STR "$ZCONVERT. Range is -" MAX_NEG " to " MAX_POS

typedef enum
{
	TYPE_HEX,
	TYPE_DEC_POS,
	TYPE_DEC_NEG,
	INV
} conv_type;
#define MAX_HEX_LEN MAX_HEX_DIGITS_IN_INT8
#define MAX_DEC_LEN MAX_DIGITS_IN_INT8

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
	intrpt_state_t  prev_intrpt_state;

	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);

	MV_FORCE_STR(kase);
	if (-1 == (index = verify_case(&kase->str)))
	{
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCASECODE, 2, kase->str.len, kase->str.addr);
	}

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
		if (!badchar_inhibit)
			MV_FORCE_LEN_STRICT(src);
		else
			MV_FORCE_LEN(src);
		if (2 * src->str.char_len <= MAX_ZCONVBUFF)
		{ /* Check if the stack buffer is sufficient considering the worst case where all
		     characters are surrogate pairs, each of which needs 2 UChars */
			src_ustr_ptr = src_ustr;
			src_ustr_len = MAX_ZCONVBUFF;
		} else
		{
		  /* To avoid preflight, allocate (2 * lenght of src->str.char_len). */
			src_ustr_len = 2 * src->str.char_len;
			src_ustr_ptr = (UChar*)malloc(src_ustr_len * SIZEOF(UChar));
		}
		/* Convert UTF-8 src to UTF-16 (UChar*) representation */
		status = U_ZERO_ERROR;
		u_strFromUTF8(src_ustr_ptr, src_ustr_len, &src_chlen, src->str.addr, src->str.len, &status);
		if ((U_ILLEGAL_CHAR_FOUND == status || U_INVALID_CHAR_FOUND == status) && badchar_inhibit)
		{	/* VIEW "NOBADCHAR" in effect, use the M mode conversion only on error */
			dstlen = src->str.len;
			ENSURE_STP_FREE_SPACE(dstlen);
			dstbase = (char *)stringpool.free;
			assert(NULL != casemaps[index].m);
			(*casemaps[index].m)((unsigned char *)dstbase, (unsigned char *)src->str.addr, dstlen);
		} else if (U_FAILURE(status))
		{
			RELEASE_IF_NOT_LOCAL(src_ustr_ptr, src_ustr);
			if (U_ILLEGAL_CHAR_FOUND == status || U_INVALID_CHAR_FOUND == status)
				utf8_len_strict((unsigned char *)src->str.addr, src->str.len);	/* to report BADCHAR error */
			ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ICUERROR,
						1, status); /* ICU said bad, we say good or don't recognize error */
		} else
		{	/* BADCHAR should not be possible after the above validations */
			status = U_ZERO_ERROR;
			dst_ustr_ptr = dst_ustr;
			dst_chlen = (*casemaps[index].u)(dst_ustr_ptr, MAX_ZCONVBUFF, src_ustr_ptr, src_chlen, NULL, &status);
			if (U_BUFFER_OVERFLOW_ERROR == status)
			{
				status = U_ZERO_ERROR;
				dst_ustr_ptr = (UChar*)malloc(dst_chlen * SIZEOF(UChar));
				/* Now, perform the real conversion with sufficient buffers */
				dst_chlen = (*casemaps[index].u)(dst_ustr_ptr, dst_chlen, src_ustr_ptr, src_chlen, NULL, &status);
			} else if (U_FILE_ACCESS_ERROR == status)
			{
				RELEASE_IF_NOT_LOCAL(src_ustr_ptr, src_ustr);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ICUERROR,
					      1, status);
			}
			/* Fake the conversion from UTF-16 to UTF-8 to compute the required buffer size */
			status = U_ZERO_ERROR;
			dstlen = 0;
			u_strToUTF8(NULL, 0, &dstlen, dst_ustr_ptr, dst_chlen, &status);
			assert(U_BUFFER_OVERFLOW_ERROR == status || U_SUCCESS(status));
			if (MAX_STRLEN < dstlen)
			{
				RELEASE_IF_NOT_LOCAL(dst_ustr_ptr, dst_ustr);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MAXSTRLEN);
			}
			ENSURE_STP_FREE_SPACE(dstlen);
			dstbase = (char *)stringpool.free;
			status = U_ZERO_ERROR;
			u_strToUTF8(dstbase, dstlen, &ulen, dst_ustr_ptr, dst_chlen, &status);
			RELEASE_IF_NOT_LOCAL(src_ustr_ptr, src_ustr);
			if (U_FAILURE(status))
			{
				RELEASE_IF_NOT_LOCAL(src_ustr_ptr, src_ustr);
				RELEASE_IF_NOT_LOCAL(dst_ustr_ptr, dst_ustr);
				ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_ICUERROR,
					      1, status); /* ICU said bad, but same call above just returned OK */
			}
			assertpro(ulen == dstlen);
			RELEASE_IF_NOT_LOCAL(dst_ustr_ptr, dst_ustr);
		}
	} else
		dstbase = NULL;	/* 4SCA: Assigned value is garbage or undefined, but below memcpy protected by dstlen */
	MV_INIT_STRING(dst, dstlen, dstbase);
	stringpool.free += dstlen;
	DBG_VALIDATE_MVAL(dst);
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
}

/**************************************************************************************************
 * This routine obtains the valid dec or hex value present in the passed val argument.
 * Does so by checking size and type of individual characters based on type.
 * Its main use is for get_dec and git_hex to obtain valid decimal and hexadecimal values.
 * Parameters:
 *  	type - corresponds to the type of input passed through val argument which is either hex or dec
 *  	val - input string containing the valid value if any to be extracted
 *  	sz - size of argument passed to val
 *  	val_len - address of an integer variable meant to hold the length of valid value present in argument of val
 *	orig_val - the untrimmed input value used only in error message
 *	orig_sz	- the untrimmed input value size used only in error message
 * Returns:
 *  	starting address of the valid input present in argument of val or NULL when there is no valid input characters to convert
 *  	size of the valid input is passed through val_len
 *	error is thrown if size is greater than acceptable max values
 ***************************************************************************************************/
static inline char* get_val(conv_type *type, char *val, int sz, int *val_len, char *orig_val, int orig_sz)
{
	char	*val_ptr;
	char	*type_str;

	/* Ignore leading 0's as they don't count towards computation */
	while (sz && ('0' == *val))
	{
		val++;
		sz--;
	}
	val_ptr = val;
	if (0 == sz)
	{
		*type = INV;
		return NULL;
	}
	*val_len = sz;
	type_str = (TYPE_HEX == *type) ? H_STR : D_STR;
	/* Validates digits to be of hex type */
	if (TYPE_HEX == *type)
	{
		while (sz && ISXDIGIT(*val))
		{
			sz--;
			val++;
		}
	} else
	{	/* Validates digits to be of decimal type */
		while (sz && ISDIGIT(*val))
		{
			sz--;
			val++;
		}
	}
	/* Case: input similar to "345x32", "FFxaa"
	 * The above while loops don't traverse the complete string
	 * hence the size will not be equal to zero
	 */
	if (0 != sz)
	{	/* Case: input is similar to "kjdl", all invalid chars
		 * The while loops don't iterate over the string
		 * hence the val and val_ptr will point to the same address
		 */
		if (val == val_ptr)
		{
			*type = INV;
			return NULL;
		} else	/* Length of valid chars */
			*val_len = val - val_ptr;
	}
	/* After validating input value character set,
	 * the below conditions check if the value is in convertable range
	 */
	if (TYPE_DEC_POS == *type)
	{	/* Decimal Positive value range check. When value length is
		 * 20 the check is required here as not all values are supported at this length
		 */
		if ((MAX_DEC_LEN < *val_len) || ((MAX_DEC_LEN == *val_len) && (0 < STRNCMP_STR(val_ptr, MAX_POS, *val_len))))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_INVVALUE, 6, orig_sz, orig_val, RTS_ERROR_STRING(type_str),
				RTS_ERROR_LITERAL(DEC_RANGE_ERR_STR));
	} else if (TYPE_DEC_NEG == *type)
	{	/* Decimal Negative value range check. When value length is
		 * 19 the check is required here as not all values are supported at this length
		 */
		if (((MAX_DEC_LEN - 1) < *val_len) || (((MAX_DEC_LEN - 1) == *val_len) && (0 < STRNCMP_STR(val_ptr, MAX_NEG, *val_len))))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_INVVALUE, 6, orig_sz, orig_val, RTS_ERROR_STRING(type_str),
				RTS_ERROR_LITERAL(DEC_RANGE_ERR_STR));
	} else
	{	/* Hexadecimal max number of digits supported for conversion */
        	if (MAX_HEX_LEN < *val_len)
                	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_INVVALUE, 6, orig_sz, orig_val, RTS_ERROR_STRING(type_str),
				RTS_ERROR_LITERAL("$ZCONVERT. Range is 0 to FFFFFFFFFFFFFFFF"));
	}
	return val_ptr;
}

/**************************************************************************************************
 * This routine gets the valid dec value present in the passed val argument.
 * Does so by checking the validity of decimal input based on sign and length before passing off to the generic get_val routine.
 * Parameters:
 *  	val - input string containing the valid value if any to be extracted
 *  	sz - size of argument passed to val
 *  	val_len - address of an integer variable meant to hold the length of valid value present in argument of val
 * Returns:
 *  	starting address of the valid input present in argument of val or NULL when there is no valid input characters to convert
 *  	size of the valid input is passed through val_len
 *
 ***************************************************************************************************/
static inline char* get_dec(char *val, int sz, int *val_len, conv_type *inp_type)
{
	char 	*orig_val = val;
	int 	orig_sz = sz;

	*inp_type = TYPE_DEC_POS;
	/* Case: null input */
	if (0 == sz)
	{
		*inp_type = INV;
		return NULL;
	}
	/* Case: signed input */
	if ('-' == *val)
		*inp_type = TYPE_DEC_NEG;
	if (('+' == *val) || ('-' == *val))
	{
		sz--;
		val++;
	}
	return get_val(inp_type, val, sz, val_len, orig_val, orig_sz);
}

/**************************************************************************************************
 * This routine gets the valid hex value present in the passed val argument.
 * Does so by checking the validity of hex input based on leading 0's,
 * 0x prefix and length before passing off to the generic get_val routine.
 * Parameters:
 *  	val - input string containing the valid value if any to be extracted
 * 	sz - size of argument passed to val
 *  	val_len - address of an integer variable meant to hold the length of valid value present in argument of val
 * Returns:
 *  	starting address of the valid input present in argument of val or NULL when there is no valid input characters to convert
 *  	size of the valid input is passed through val_len
 ***************************************************************************************************/
static inline char* get_hex(char *val, int sz, int *val_len, conv_type *inp_type)
{
	char 	*orig_val = val;
	int 	orig_sz = sz;

	*inp_type = TYPE_HEX;
	/* Case: null input */
	if (0 == sz)
	{
		*inp_type = INV;
		return NULL;
	}
	/* Case: 0x or 0X are ignored as they don't count towards the value to be converted */
	if ((1 < sz) && ('0' == val[0] && ('x' == val[1] || 'X' == val[1])))
	{
		val += 2;
		sz -= 2;
	}
	return get_val(inp_type, val, sz, val_len, orig_val, orig_sz);
}


/**************************************************************************************************
 * Routine to perform conversion between UTF formats and conversions between DEC and HEX values.
 *      1. Conversions between DEC and HEX are performed by first checking the validity of passed src
 *	   argument and then converting them to the right type.
 *      2. Conversions betweeen UTF formats is carried out using ICU API
 *	3. Result in both cases are copied as an mval to dst
 * Parameters:
 * 	src - holds the value to be converted
 * 	ichset - category of src value to be converted from
 * 	ochset - category of dst value to be converted to
 *	dst - holds the address to which converted value is copied
 * Returns:
 *	Converted value through dst
 * Notice:
 *	DEC to HEX conversion can handle 19 digit signed integer values
 *	HEX to DEC conversion can handle 16 digit unsigned hex values
 *	Negative number DEC to HEX conversion, uses 2's complement to represent negative hex result
 *	Invalid input results in a default value return which is 0
 ***************************************************************************************************/
void	op_fnzconvert3(mval *src, mval* ichset, mval* ochset, mval* dst)
{
	/* UTF specific */
	UConverter	*from, *to;
	/* DEC & HEX conversions specific */
	gtm_chset_t	fmode, tmode;
	char 		*val_ptr;
	unsigned char	*strpool_ptr, *str_pool;	/* Local stringpool pointers */
	qw_num		i8val;
	int		strpool_len, dstlen, len;
	boolean_t	is_dh_type = FALSE;
	conv_type	inp_type;

	MV_FORCE_STR(src);
	MV_FORCE_STR(ichset);
        MV_FORCE_STR(ochset);
	fmode = verify_chset(&ichset->str);
	if (0 > fmode)		/* Validating input CHSET */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2, ichset->str.len, ichset->str.addr);
	tmode = verify_chset(&ochset->str);
	if (0 > tmode)		/* Validating output CHSET */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2, ochset->str.len, ochset->str.addr);
	/* Check if the input and output CHSET is of type DEC or HEX */
	if (((CHSET_DEC == fmode) || (CHSET_HEX == fmode)) && ((CHSET_HEX == tmode) || (CHSET_DEC == tmode)))
	{
		if ((CHSET_DEC == fmode) && (CHSET_HEX == tmode))
		{ 	/* This block handles DEC to HEX conversion */
			if (NULL == (val_ptr = get_dec(src->str.addr, src->str.len, &dstlen, &inp_type)))	/* Warning: assignment */
			{	/* Default output on invalid input */
				val_ptr = "0";
				dstlen = 1;
			}
			i8val = asc2l((uchar_ptr_t)val_ptr, dstlen);
			/* Handling -ve number input by computing the 2's complement and converting that to hex.
			 * Also verify source length > 0 since we allow a null input.
			 */
			if (TYPE_DEC_NEG == inp_type)
				/* As INT8 is valid on all currently supported platforms.
				 * There is no need for additional handling here.
				 */
				i8val = ((~i8val) + 1);
			/* When the dstlen is 1 we need an additional character space
			 * to prefix F for hexadecimal negative numbers ranging 0-7
			 */
			if ((TYPE_DEC_NEG == inp_type) && (dstlen == 1))
				dstlen = dstlen + 1;
			ENSURE_STP_FREE_SPACE(dstlen);
			strpool_len = dstlen;
			str_pool = stringpool.free;
			i2hexl(i8val, str_pool, dstlen);
			while ((1 < dstlen) && ('0' == *str_pool))	/* Trimming the output of leading zero's */
			{
				str_pool++;
				dstlen--;
			}
			/* Incase of -ve numbers 2's complement creates extra F's it is desirable to trim the value */
			if (TYPE_DEC_NEG == inp_type)
			{
				len = dstlen;
				strpool_ptr = str_pool;
				while (1 < len && ('F' == *strpool_ptr))
				{
					strpool_ptr++;
					len--;
				}
				if (0 < len)
				{	/* As values 8-E in the highest order digit represents
					 * negative hex values, F is not required to be prefixed here.
					 */
					if ('7' < *strpool_ptr)
					{
						str_pool = strpool_ptr;
						dstlen = len;
					} else
					{	/* Value of the highest order digit if less than 7 needs additional F to represent
						 * negativity. So the pointer is moved back one to include F.
						 */
						str_pool = strpool_ptr - 1;
                	                 	dstlen = len + 1;
					}
				}
			}
			MV_INIT_STRING(dst, dstlen, str_pool);
			stringpool.free += strpool_len;
			DBG_VALIDATE_MVAL(dst);
		} else if ((CHSET_HEX == fmode) && (CHSET_DEC == tmode))
		{	/* This block handles HEX to DEC conversion */
			if (NULL == (val_ptr = get_hex(src->str.addr, src->str.len, &dstlen, &inp_type)))	/* Warning: assignment */
        		{	/* Default output on invalid input */
				val_ptr = "0";
				dstlen = 1;
                	}
			i8val = asc_hex2l((uchar_ptr_t)val_ptr, dstlen);	/* asc_hex2l does its own case conversion */
			ui82mval(dst, i8val);
			DBG_VALIDATE_MVAL(dst);
		} else	/* Invalid input and output CHSET combination DEC,DEC or HEX,HEX */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVZCONVERT, 4,
				ichset->str.len, ichset->str.addr, ochset->str.len, ochset->str.addr);
	} else if (gtm_utf8_mode)
	{	/* UTF Family of input */
		/* The only supported names are: "UTF-8", "UTF-16", "UTF-16LE" and "UTF-16BE */
		if (NULL == (from = get_chset_desc(&ichset->str)))	/* Warning: assignment */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2, ichset->str.len, ichset->str.addr);
		if (NULL == (to = get_chset_desc(&ochset->str)))	/* Warning: assignment */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2, ochset->str.len, ochset->str.addr);
		dstlen = gtm_conv(from, to, &src->str, NULL, NULL);
		assert(0 <= dstlen);	/* Should be positive and non-zero since args were validated above */
		MV_INIT_STRING(dst, dstlen, stringpool.free);
		stringpool.free += dstlen;
		DBG_VALIDATE_MVAL(dst);
	} else	/* In a NON-UTF mode UTF Family of CHSET is used */
		/* Report error as the input and output categories are not supported in this context */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_INVZCONVERT, 4,
			ichset->str.len, ichset->str.addr, ochset->str.len, ochset->str.addr);
}
