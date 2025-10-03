/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "min_max.h"
#include "op.h"
#include "toktyp.h"
#include "gtm_ctype.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#endif

#ifdef UTF8_SUPPORTED
#include "hashtab_int4.h"
#include "hashtab.h"
#include "gtm_utf8.h"
#endif

GBLREF spdesc		stringpool;
GBLREF boolean_t	gtm_utf8_mode;

#ifdef UTF8_SUPPORTED
GBLREF	boolean_t	badchar_inhibit;

error_def(ERR_MAXSTRLEN);
#endif

#define GET_DIRERCTION(SRC, DST)								\
{												\
	unsigned char		ch;								\
												\
	if (1 != (SRC)->str.len)								\
		*(DST) = NO_DIR;								\
	else											\
	{											\
		ch = TOUPPER((SRC)->str.addr[0]);						\
		if ('R' == ch)									\
			*(DST) = RIGHT_DIR;							\
		else if ('L' == ch)								\
			*(DST) = LEFT_DIR;							\
		else if ('B' == ch)								\
			*(DST) = BOTH_DIR;							\
		else										\
			*(DST) = NO_DIR;							\
	}											\
}

#define ITER_ZTRANSLATE_VALUES(START, END, SRC, XLATE, DST, SHOULD_ITERATE)			\
{												\
	int		i;									\
	unsigned char	value;									\
												\
	if (SHOULD_ITERATE)									\
	{											\
		for (i = START; i < END; i++)							\
		{										\
			value = (unsigned char)((SRC)->str.addr[i]);				\
			if (DELETE_VALUE != XLATE[value])					\
				*DST++ = (NO_VALUE == XLATE[value]) ? value : XLATE[value];	\
		}										\
	}											\
}

#ifdef UTF8_SUPPORTED
#define SET_BOUND(START, END, PTR, DIRECTION)							\
{ 												\
	if (NULL == START)									\
	{											\
		START = END = PTR;								\
		if (LEFT_DIR == DIRECTION)							\
			break;									\
	} else											\
		END = PTR;									\
}

#define ITER_BYTE_CHAR(SRC, PREV, LEN, DST, CHAR_LEN)						\
{												\
	LEN = SRC - PREV;									\
	copy_character(LEN, DST, PREV);								\
	CHAR_LEN++;										\
}

#define ITER_UTF8_CHAR(CURR, PTR, OFFSET, LEN, TOP, CODE, DST, CHAR_LEN)			\
{												\
	CURR = &PTR[OFFSET];									\
	LEN = (char *)UTF8_MBTOWC(CURR, TOP, CODE) - CURR;					\
	copy_character(LEN, DST, CURR);								\
	CHAR_LEN++;										\
}
/* This module implements a variety of approaches:
 * fnz entries expect that at least the search and replace strings are single byte
 * "fast" entries receive translation tables prepared from literals by the compilers, while the others prepare them at run time
 * Both fn and fnz entries finish with common scanning and replacement for "fast" and non-fast entries
 * pure UTF-8 entries deal with the possibility all arguments might have multi-byte characters, and if appropriate, use a hash table
 *  to represent the search string
 * The fnz entries assume at least the search and replace strings contain no multi-byte characters
 * If the compiler detects (in f_translate) all the arguments are constants, it creates the translation tables and invokes this
 *  routine winding up in the "common" code to obtain a literal result
 * If the compiler detects the search and replace arguments are constants, it preconstructs the translation tables, embeds them as
 *  literals in the object and generates code to use the "fast" entry points
 */

void op_fntranslate_common(mval *src, mval *dst, mval *rplc, int4 *xlate, hash_table_int4 *xlate_hash, translate_direction dir)
{
	boolean_t		copy_orig_char = FALSE, single_byte_src;
	char			*prevsrc, *rcur, *rptr, *rtop, *srcptr, *srctop;
	char			*left_bound = NULL, *right_bound = NULL;
	ht_ent_int4		*tabent;
	int4			offset;
	sm_uc_ptr_t		dstptr;
	uint4			char_len, code, copy_length, dstlen;
	unsigned char		value;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* callers using stringpool (compile_time) must already make sure there is space in the stringpool for the result */
	if (!IS_IN_STRINGPOOL(xlate_hash, 1))					/* length not available but doesn't matter */
	{
		ENSURE_STP_FREE_SPACE(src->str.char_len * MAX_CHAR_LEN);
	} else	/* nasty things can happen */
		assertpro(IS_STP_SPACE_AVAILABLE_PRO(src->str.char_len * MAX_CHAR_LEN));
	single_byte_src = (src->str.len == src->str.char_len);
	if (NO_DIR != dir)
	{
		for (srcptr = src->str.addr, srctop = src->str.addr + src->str.len; srcptr < srctop; )
		{
			prevsrc = srcptr;
			srcptr = (TRUE == single_byte_src) ? (srcptr + 1) : (char *)UTF8_MBTOWC(srcptr, srctop, code);
			if (single_byte_src || (1 == (srcptr - prevsrc)))
			{
				value = *prevsrc;
				assert(xlate[value] < rplc->str.len);
				if (NO_VALUE == xlate[value])
					SET_BOUND(left_bound, right_bound, prevsrc, dir);
			} else if (NULL != xlate_hash)
			{
				if (WEOF == code)
					break;
				tabent = (ht_ent_int4 *)lookup_hashtab_int4(xlate_hash, (uint4*)&code);
				if (NULL == tabent)	/* Code not found */
					SET_BOUND(left_bound, right_bound, prevsrc, dir);
			} else				/* Code not found */
					SET_BOUND(left_bound, right_bound, prevsrc, dir);
		}
	}
	dstptr = stringpool.free;
	dstlen = char_len = 0;
	for (srcptr = src->str.addr, srctop = src->str.addr + src->str.len,
			rptr = rplc->str.addr, rtop = rplc->str.addr + rplc->str.len; srcptr < srctop; )
	{
		if (NULL != left_bound)
		{
			copy_orig_char = (((BOTH_DIR == dir) && (srcptr < right_bound) && (srcptr >= left_bound)) ||
					((RIGHT_DIR == dir) && (srcptr < right_bound)) ||
					((LEFT_DIR == dir) && (srcptr >= left_bound))) ? TRUE : FALSE;
		}
		prevsrc = srcptr;
		srcptr = (TRUE == single_byte_src) ? (srcptr + 1) : (char *)UTF8_MBTOWC(srcptr, srctop, code);
		copy_length = 0;
		if (single_byte_src || (1 == (srcptr - prevsrc)))
		{
			value = *prevsrc;
			offset = xlate[value];
			assert(offset < rplc->str.len);
			if ((TRUE == copy_orig_char) || (NO_VALUE == offset))
			{
				*dstptr = value;
				copy_length = 1;
				char_len++;
			} else if (DELETE_VALUE != offset)
			{
				assert(0 <= offset);
				ITER_UTF8_CHAR(rcur, rptr, offset, copy_length, rtop, code, dstptr, char_len);
			} /* else offset is DELETE_VALUE and copy_orig_char is false - do nothing */
		} else if (NULL != xlate_hash)
		{
			if (WEOF == code)
				break;
			tabent = (ht_ent_int4 *)lookup_hashtab_int4(xlate_hash, (uint4*)&code);
			if ((TRUE == copy_orig_char) || (NULL == tabent))
			{	/* Code not found, copy over value to string */
				ITER_BYTE_CHAR(srcptr, prevsrc, copy_length, dstptr, char_len);
			} else
			{
				offset = (int4)((size_t)(tabent->value));
				assert(0 <= offset);
				if (MAXPOSINT4 != offset)
				{	/* Valid translation found; copy new value to dst */
					/* Because the hashtable can't do zeros, it has the offset incremented by one, compensate */
					ITER_UTF8_CHAR(rcur, rptr, (offset - 1), copy_length, rtop, code, dstptr, char_len);
				}
			}
		} else
		{	/* if null == xlate_hash, mappings are single character, so no sense looking it up; simply copy it */
			ITER_BYTE_CHAR(srcptr, prevsrc, copy_length, dstptr, char_len);
		}
		dstptr += copy_length;
		assert(dstptr <= stringpool.top);
		dstlen += copy_length;
		if (MAX_STRLEN < dstlen)
		{
			if (TREF(compile_time))
				stx_error(VARLSTCNT(1) ERR_MAXSTRLEN);
			else
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MAXSTRLEN);
		}
	}
	MV_INIT_STRING(dst, dstlen, stringpool.free);
	dst->mvtype &= ~MV_UTF_LEN;	/* character length unknown because translation may modify effective UTF representation */
	stringpool.free = (unsigned char *)dstptr;
}

void op_fntranslate(mval *src, mval *srch, mval *rplc, mval *dir, mval *dst)
{
	int			maxLengthString, *xlate;
	mstr			m_xlate;
	static hash_table_int4	*xlate_hash = NULL;
	static int4		xlate_array[NUM_CHARS];
	static unsigned int 	prev_gcols = -1;
	static mstr		prev_srch = {0, 0}, prev_rplc = {0, 0};
	translate_direction	direction;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gtm_utf8_mode && !TREF(compile_time));				/* compiler only directs this for UTF-8) */
	MV_FORCE_STR(src);							/* ensure all args have string representations */
	MV_FORCE_STR(srch);
	MV_FORCE_STR(rplc);
	MV_FORCE_STR(dir);
	if (!badchar_inhibit)
	{       /* needed only to validate for BADCHARs */
		MV_FORCE_LEN(src);
		MV_FORCE_LEN(srch);
		MV_FORCE_LEN(rplc);
		MV_FORCE_LEN(dir);
	} else
	{	/* but need some at least sorta valid length */
		MV_FORCE_LEN_SILENT(src);
		MV_FORCE_LEN_SILENT(srch);
		MV_FORCE_LEN_SILENT(rplc);
		MV_FORCE_LEN_SILENT(dir);
	}

	assert((0 <= src->str.char_len) && (MAX_STRLEN >= src->str.char_len));
	assert((0 <= srch->str.char_len) && (MAX_STRLEN >= srch->str.char_len));
	assert((0 <= rplc->str.char_len) && (MAX_STRLEN >= rplc->str.char_len));
	assert((0 <= dir->str.char_len) && (MAX_STRLEN >= dir->str.char_len));
	if ((srch->str.len == srch->str.char_len) && (rplc->str.len == rplc->str.char_len))
	{	/* single character srch & rplc allow use of straight code table */
		if (!((prev_gcols == stringpool.gcols) && (srch->str.addr == prev_srch.addr) && (srch->str.len == prev_srch.len)
				&& (rplc->str.addr == prev_rplc.addr) && (rplc->str.len == prev_rplc.len)))
			op_fnztranslate(src, srch, rplc, dir, dst);
		return;
	}
	maxLengthString = srch->str.char_len;
	if (!((srch->mvtype & MV_UTF_LEN) && (srch->str.len == srch->str.char_len)) && (NUM_CHARS < srch->str.char_len))
	{	/* need more space */
		m_xlate.len = srch->str.char_len;
		m_xlate.addr = malloc(SIZEOF(int4) * maxLengthString);
		maxLengthString += MIN((src->str.char_len * MAX_CHAR_LEN), MAX_STRLEN);
	} else
	{
		m_xlate.len = SIZEOF(int4) * NUM_CHARS;
		m_xlate.addr = (char *)xlate_array;
	}
	ENSURE_STP_FREE_SPACE(maxLengthString); /* do now because op_fntranslate_common must be paranoid about stp_gcol */
	if (!((prev_gcols == stringpool.gcols) && (&xlate_array[0] == (int4 *)m_xlate.addr) && (srch->str.addr == prev_srch.addr)
		&& (srch->str.len == prev_srch.len) && (rplc->str.addr == prev_rplc.addr) && (rplc->str.len == prev_rplc.len)))
	{	/* not a repeat, so can't reuse the last tables; above: !&& fails quicker than || */
		if (NULL != xlate_hash)
		{	/* about to allocate new one, so free any old; done after error checks to keep pointer valid */
			free_hashtab_int4(xlate_hash);
			free(xlate_hash);
		}
		xlate_hash = create_utf8_xlate_table(srch, rplc, &m_xlate);
		prev_gcols = stringpool.gcols;
		prev_srch = srch->str;
		prev_rplc = rplc->str;
	}
	GET_DIRERCTION(dir, &direction)
	op_fntranslate_common(src, dst, rplc, (int4 *)m_xlate.addr, xlate_hash, direction);
	if (&xlate_array[0] != (int4 *)m_xlate.addr)
		free(m_xlate.addr);
}

void op_fntranslate_fast(mval *src, mval *rplc, mval *m_xlate, mval *dir, mval *m_xlate_hash, mval *dst)
{
	hash_table_int4		*xlate_hash;  /* translation table to hold all multi-byte character mappings */
	int4			*xlate;
	translate_direction	direction;
	int			size_gcol;

	assert(gtm_utf8_mode);							/* compiler only uses this for UTF-8) */
	assert(MV_STR & m_xlate->mvtype);
	MV_FORCE_STR(src);							/* ensure string representations */
	MV_FORCE_STR(rplc);
	MV_FORCE_STR(dir);
	if (!badchar_inhibit)
	{       /* needed only to validate for BADCHARs */
		MV_FORCE_LEN(src);
		MV_FORCE_LEN(rplc);
		MV_FORCE_LEN(dir);
	} else
	{	/* but need some at least sorta valid length */
		MV_FORCE_LEN_SILENT(src);
		MV_FORCE_LEN_SILENT(rplc);
		MV_FORCE_LEN_SILENT(dir);
	}
	/* ensure space for dst without stp_gcol moving xlate_table; src can only increase from current to maximum byte length */
	size_gcol = ((gtm_utf8_mode ? (src->str.char_len * MAX_CHAR_LEN) : src->str.len) + m_xlate->str.len);
	ENSURE_STP_FREE_SPACE(size_gcol);
#	ifdef DEBUG
	if ((WBTEST_ENABLED(WBTEST_GCOL)) && (2 == gtm_white_box_test_case_count))
		INVOKE_STP_GCOL(size_gcol);
#	endif
	xlate = (int4 *)m_xlate->str.addr;
	xlate_hash = m_xlate_hash->str.len ? activate_hashtab_in_buffer_int4((sm_uc_ptr_t)m_xlate_hash->str.addr, NULL) : NULL;
	GET_DIRERCTION(dir, &direction)
	op_fntranslate_common(src, dst, rplc, xlate, xlate_hash, direction);
}
#endif /* UTF8_SUPPORTED */

/* $ZTRANSLATE() is implemented using a byte-indexed translation table xlate[NUM_CHARS] which stores the
 * replacement character (byte) for a given character (byte) of the second argument specified in $TR().
 */
void op_fnztranslate(mval *src, mval *srch, mval *rplc, mval *dir, mval *dst)
{
	static int		xlate[NUM_CHARS];		/* not STATICDEF to prevent conflict with op_fn */
	static mstr		prev_srch = {0, 0}, prev_rplc = {0, 0};
	static unsigned int	prev_gcols = ~0x0;
	translate_direction	direction;

	MV_FORCE_STR(src);
	MV_FORCE_STR(srch);
	MV_FORCE_STR(rplc);
	MV_FORCE_STR(dir);
	if (gtm_utf8_mode)
		MV_FORCE_LEN_SILENT(src);
	if (!(IS_STP_SPACE_AVAILABLE((src->str.len)) && (prev_gcols == stringpool.gcols) && (srch->str.addr == prev_srch.addr)
		&& (srch->str.len == prev_srch.len) && (rplc->str.addr == prev_rplc.addr) && (rplc->str.len == prev_rplc.len)))
	{
		create_byte_xlate_table(srch, rplc, xlate);
		prev_gcols = stringpool.gcols;
		prev_srch = srch->str;
		prev_rplc = rplc->str;
	}
	ENSURE_STP_FREE_SPACE(src->str.len);
	GET_DIRERCTION(dir, &direction)
	op_fnztranslate_common(src, dst, xlate, direction);
}

void op_fnztranslate_common(mval *src, mval *dst, int *xlate, translate_direction dir)
{
	boolean_t	rplc_vals_from_left, rplc_vals_from_right, no_match;
	int		i, left_bound, right_bound;
	unsigned char	*dstptr, value;

	/* Callers are responsible for making sure there is space in the stringpool for the result,
	 * with byte operation dst len cannot exceed src len secured by caller */
	assert(IS_STP_SPACE_AVAILABLE(src->str.len));
	dst->mvtype = 0;
	dstptr = stringpool.free;
	if (NO_DIR != dir)
	{
		no_match = FALSE;
		left_bound = (-1);
		right_bound = (-1);
		/* If there are no values to replace, no need to iterate again, just memcpy() */
		rplc_vals_from_left = FALSE;
		rplc_vals_from_right = FALSE;
		if (RIGHT_DIR != dir)
		{	/* LEFT_DIR or BOTH_DIR */
			for (i = 0; i < src->str.len; i++)
			{
				value = (unsigned char)src->str.addr[i];
				if (NO_VALUE == xlate[value])
				{
					left_bound = i;
					break;
				} else if (DELETE_VALUE != xlate[value])
					rplc_vals_from_left = TRUE;
			}
			if ((-1) == left_bound)
				/* Avoid looping again if BOTH_DIR */
				no_match = TRUE;
		}
		if ((LEFT_DIR != dir) && (FALSE == no_match))
		{	/* RIGHT_DIR or BOTH_DIR */
			for (i = (src->str.len - 1); 0 <= i; i--)
			{
				value = (unsigned char)src->str.addr[i];
				if (NO_VALUE == xlate[value])
				{
					right_bound = i;
					break;
				} else if (DELETE_VALUE != xlate[value])
					rplc_vals_from_right = TRUE;
			}
			if ((-1) == right_bound)
				no_match = TRUE;
		}
		if (no_match)
		{	/* Couldn't find a stopper, so regular iteration */
			ITER_ZTRANSLATE_VALUES(0, src->str.len, src, xlate, dstptr, (rplc_vals_from_left || rplc_vals_from_right));
		} else if (LEFT_DIR == dir)
		{
			ITER_ZTRANSLATE_VALUES(0, left_bound, src, xlate, dstptr, rplc_vals_from_left);
			memcpy(dstptr, (src->str.addr + left_bound), (src->str.len - left_bound));
			dstptr += (src->str.len - left_bound);
		} else if (RIGHT_DIR == dir)
		{
			memcpy(dstptr, src->str.addr, (right_bound + 1));
			dstptr += (right_bound + 1);
			ITER_ZTRANSLATE_VALUES((right_bound + 1), src->str.len, src, xlate, dstptr, rplc_vals_from_right);
		} else /* BOTH_DIR */
		{
			ITER_ZTRANSLATE_VALUES(0, left_bound, src, xlate, dstptr, rplc_vals_from_left);
			memcpy(dstptr, (src->str.addr + left_bound), (right_bound - left_bound + 1));
			dstptr += (right_bound - left_bound + 1);
			ITER_ZTRANSLATE_VALUES((right_bound + 1), src->str.len, src, xlate, dstptr, rplc_vals_from_right);
		}
	} else
	{	/* Regular $ztranslate */
		ITER_ZTRANSLATE_VALUES(0, src->str.len, src, xlate, dstptr, TRUE);
	}
	assert(dstptr <= stringpool.top);
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST((char *)dstptr - dst->str.addr);
	dst->mvtype = MV_STR;
	stringpool.free = (unsigned char *)dstptr;
}

void op_fnztranslate_fast(mval *src, mval *m_xlate, mval *dir, mval *dst)
{
	int4			*xlate;
	translate_direction	direction;

	assert(MV_STR & m_xlate->mvtype);
	MV_FORCE_STR(src);
	MV_FORCE_STR(dir);
	if (gtm_utf8_mode)
		MV_FORCE_LEN_SILENT(src);
	ENSURE_STP_FREE_SPACE(src->str.len); /* Allocate string space now so the stringpool doesn't shift xlate table */
	xlate = (int4 *)m_xlate->str.addr;
	assert(m_xlate->str.len == NUM_CHARS * SIZEOF(int4));
	GET_DIRERCTION(dir, &direction)
	op_fnztranslate_common(src, dst, xlate, direction);
}
