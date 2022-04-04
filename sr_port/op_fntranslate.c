/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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

GBLREF spdesc stringpool;

#ifdef UTF8_SUPPORTED
#include "hashtab_int4.h"
#include "hashtab.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	badchar_inhibit, gtm_utf8_mode;

error_def(ERR_MAXSTRLEN);

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

void op_fntranslate_common(mval *src, mval *dst, mval *rplc, int4 *xlate, hash_table_int4 *xlate_hash)
{
	boolean_t	single_byte_src;
	char		*prevsrc, *rcur, *rstr, *rtop, *srcptr, *srctop;
	ht_ent_int4	*tabent;
	int4 		offset;
	sm_uc_ptr_t	dstptr;
	uint4		char_len, code, copy_length, dstlen;
	unsigned char	val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* callers using stringpool (compile_time) must already make sure there is space in the stringpool for the result */
	if (!IS_IN_STRINGPOOL(xlate_hash, 1))					/* length not available but doesn't matter */
	{
		ENSURE_STP_FREE_SPACE(src->str.char_len * MAX_CHAR_LEN);
	} else	/* nasty things can happen */
		assertpro(IS_STP_SPACE_AVAILABLE_PRO(src->str.char_len * MAX_CHAR_LEN));
	srcptr = src->str.addr;
	srctop = srcptr + src->str.len;
	dstptr = stringpool.free;
	dstlen = char_len = 0;
	rstr = rplc->str.addr;
	rtop = rplc->str.addr + rplc->str.len;
	single_byte_src = src->str.len == src->str.char_len;
	while (srcptr < srctop)
	{
		if (single_byte_src)
		{
			prevsrc = srcptr++;
		} else
		{
			prevsrc = srcptr;
			srcptr = (char *)UTF8_MBTOWC(srcptr, srctop, code);
		}
		copy_length = 0;
		if (single_byte_src || (1 == (srcptr - prevsrc)))
		{
			val = *prevsrc;
			offset = xlate[val];
			assert(offset < rplc->str.len);
			if (DELETE_VALUE != offset)
			{
				if (NO_VALUE == offset)
				{
					*dstptr = val;
					copy_length = 1;
				} else
				{
					assert(0 <= offset);
					rcur = &rstr[offset];
					copy_length = (char *)UTF8_MBTOWC(rcur, rtop, code) - rcur;
					copy_character(copy_length, dstptr, rcur);
				}
				char_len++;
			}
		} else if (NULL != xlate_hash)
		{	/* if null == xlate_hash, mappings are single character, so no sense looking it up; simply copy it */
			if (WEOF == code)
				break;
			tabent = (ht_ent_int4 *)lookup_hashtab_int4(xlate_hash, (uint4*)&code);
			if (NULL == tabent)
			{	/* Code not found, copy over value to string */
				copy_length = srcptr - prevsrc;
				copy_character(copy_length, dstptr, prevsrc);
				char_len++;
			} else
			{
				offset = (int4)((size_t)(tabent->value));
				assert(0 <= offset);
				if (MAXPOSINT4 != offset)
				{	/* Valid translation found; copy new value to dst */
					/* Because the hashtable can't do zeros, it has the offset incremented by one,
					 *  compensate */
					rcur = &rstr[offset - 1];
					copy_length = (char *)UTF8_MBTOWC(rcur, rtop, code) - rcur;
					copy_character(copy_length, dstptr, rcur);
					char_len++;
				}
			}
		} else
		{
			copy_length = srcptr - prevsrc;
			copy_character(copy_length, dstptr, prevsrc);
			char_len++;
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

void op_fntranslate(mval *src, mval *srch, mval *rplc, mval *dst)
{
	int			dummy_len, maxLengthString, *xlate;
	mstr			m_xlate;
	static hash_table_int4	*xlate_hash = NULL;
	static int4		xlate_array[NUM_CHARS];
	static unsigned int 	prev_gcols = -1;
	static mstr		prev_srch = {0, 0}, prev_rplc = {0, 0};
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gtm_utf8_mode && !TREF(compile_time));				/* compiler only directs this for UTF-8) */
	MV_FORCE_STR(src);							/* ensure all args have string representations */
	MV_FORCE_STR(srch);
	MV_FORCE_STR(rplc);
	if (!badchar_inhibit)
	{       /* needed only to validate for BADCHARs */
		MV_FORCE_LEN(src);
		MV_FORCE_LEN(srch);
		MV_FORCE_LEN(rplc);
	} else
	{	/* but need some at least sorta valid length */
		MV_FORCE_LEN_SILENT(src);
		MV_FORCE_LEN_SILENT(srch);
		MV_FORCE_LEN_SILENT(rplc);
	}
	assert((0 <= src->str.char_len) && (MAX_STRLEN >= src->str.char_len));
	assert((0 <= srch->str.char_len) && (MAX_STRLEN >= srch->str.char_len));
	assert((0 <= rplc->str.char_len) && (MAX_STRLEN >= rplc->str.char_len));
	if ((srch->str.len == srch->str.char_len) && (rplc->str.len == rplc->str.char_len))
	{	/* single character srch & rplc allow use of straight code table */
		if (!((prev_gcols == stringpool.gcols) && (srch->str.addr == prev_srch.addr) && (srch->str.len == prev_srch.len)
				&& (rplc->str.addr == prev_rplc.addr) && (rplc->str.len == prev_rplc.len)))
			op_fnztranslate(src, srch, rplc, dst);
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
	op_fntranslate_common(src, dst, rplc, (int4 *)m_xlate.addr, xlate_hash);
	if (&xlate_array[0] != (int4 *)m_xlate.addr)
		free(m_xlate.addr);
}

void op_fntranslate_fast(mval *src, mval *rplc, mval *m_xlate, mval *m_xlate_hash, mval *dst)
{
	hash_table_int4	*xlate_hash;  /* translation table to hold all multi-byte character mappings */
	int4 *xlate;

	assert(gtm_utf8_mode);							/* compiler only uses this for UTF-8) */
	assert(MV_STR & m_xlate->mvtype);
	MV_FORCE_STR(src);							/* ensure string representations */
	MV_FORCE_STR(rplc);
	if (!badchar_inhibit)
	{       /* needed only to validate for BADCHARs */
		MV_FORCE_LEN(src);
		MV_FORCE_LEN(rplc);
	} else
	{	/* but need some at least sorta valid length */
		MV_FORCE_LEN_SILENT(src);
		MV_FORCE_LEN_SILENT(rplc);
	}
	/* ensure space for dst without stp_gcol moving xlate_table; src can only increase from current to maximum byte length */
	ENSURE_STP_FREE_SPACE((gtm_utf8_mode ? (src->str.char_len * MAX_CHAR_LEN) : src->str.len) + m_xlate->str.len);
	xlate = (int4 *)m_xlate->str.addr;
	xlate_hash = m_xlate_hash->str.len ? activate_hashtab_in_buffer_int4((sm_uc_ptr_t)m_xlate_hash->str.addr, NULL) : NULL;
	op_fntranslate_common(src, dst, rplc, xlate, xlate_hash);
}
#endif /* UTF8_SUPPORTED */

void op_fnztranslate_common(mval *src, mval *dst, int *xlate)
{
	char 		*prevsrc, *srcptr, *srctop;
	int4		n;
	uint4		char_len, code, copy_length;
	unsigned char	*dstptr, val;

	/* Callers are responsible for making sure there is space in the stringpool for the result */
	assert(IS_STP_SPACE_AVAILABLE(src->str.len));	/* with byte operation dst len cannot exceed src len secured by caller */
	dst->mvtype = 0;
	dstptr = stringpool.free;
	srcptr = src->str.addr;
	srctop = srcptr + src->str.len;
	while (srcptr < srctop)
	{
		val = *srcptr;
		n = xlate[val];
		if (DELETE_VALUE != n)
		{
			if (NO_VALUE == n)
				*dstptr = val;
			else
				*dstptr = n;
			dstptr++;
		}
		srcptr++;
	}
	assert(dstptr <= stringpool.top);
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST((char *)dstptr - dst->str.addr);
	dst->mvtype = MV_STR;
	stringpool.free = (unsigned char*)dstptr;
}

/* $ZTRANSLATE() is implemented using a byte-indexed translation table xlate[NUM_CHARS] which stores the
 * replacement character (byte) for a given character (byte) of the second argument specified in $TR().
 */
void op_fnztranslate(mval *src, mval *srch, mval *rplc, mval *dst)
{
	static int xlate[NUM_CHARS];					/* not STATICDEF to prevent conflict with op_fn */
	static mstr prev_srch = {0, 0}, prev_rplc = {0, 0};
	static unsigned int prev_gcols = -1;

	MV_FORCE_STR(src);
	MV_FORCE_STR(srch);
	MV_FORCE_STR(rplc);
	if (gtm_utf8_mode && !badchar_inhibit)
		MV_FORCE_LEN(src);
	if (!(IS_STP_SPACE_AVAILABLE((src->str.len)) && (prev_gcols == stringpool.gcols) && (srch->str.addr == prev_srch.addr)
		&& (srch->str.len == prev_srch.len) && (rplc->str.addr == prev_rplc.addr) && (rplc->str.len == prev_rplc.len)))
	{
		create_byte_xlate_table(srch, rplc, xlate);
		prev_gcols = stringpool.gcols;
		prev_srch = srch->str;
		prev_rplc = rplc->str;
	}
	ENSURE_STP_FREE_SPACE(src->str.len);
	op_fnztranslate_common(src, dst, xlate);
}

void op_fnztranslate_fast(mval *src, mval *m_xlate, mval *dst)
{
	int4		*xlate;

	assert(MV_STR & m_xlate->mvtype);
	MV_FORCE_STR(src);
	if (gtm_utf8_mode && !badchar_inhibit)
		MV_FORCE_LEN(src);
	ENSURE_STP_FREE_SPACE(src->str.len); /* Allocate string space now so the stringpool doesn't shift xlate table */
	xlate = (int4 *)m_xlate->str.addr;
	assert(m_xlate->str.len == NUM_CHARS * SIZEOF(int4));
	op_fnztranslate_common(src, dst, xlate);
}
