/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
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

#include "stringpool.h"
#include "min_max.h"
#include "op.h"
#include "toktyp.h"

GBLREF spdesc stringpool;

#ifdef UTF8_SUPPORTED
#include "hashtab_int4.h"
#include "hashtab.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	badchar_inhibit;

error_def(ERR_MAXSTRLEN);

void op_fntranslate_common(mval *src, mval *dst, mval *rplc, int4 *xlate, hash_table_int4 *xlate_hash)
{
	int4 offset;
	unsigned char val;
	char *srcptr, *srctop, *prevsrc, *rstr, *rtop, *rcur;
	sm_uc_ptr_t dstptr;
	uint4 code, copy_length, char_len, dstlen;
	ht_ent_int4 *tabent;
	boolean_t single_byte_src;

	/* Callers are responsible for making sure there is space in the stringpool for the result */
	assert(IS_STP_SPACE_AVAILABLE(4 * src->str.len));
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
				}
				else
				{
					rcur = &rstr[offset];
					copy_length = (char *)UTF8_MBTOWC(rcur, rtop, code) - rcur;
					copy_character(copy_length, dstptr, rcur);
				}
				char_len++;
			}
		} else if (NULL != xlate_hash)
		{	/* if null == xlate_hash, we know are mappings are single character, so no sense looking it up; simply
			    copy it */
			if (WEOF == code)
				continue;
			tabent = (ht_ent_int4*)lookup_hashtab_int4(xlate_hash, (uint4*)&code);
			if (NULL == tabent)
			{	/* Code not found, copy over value to string */
				copy_length = srcptr - prevsrc;
				copy_character(copy_length, dstptr, prevsrc);
				char_len++;
			} else
			{
				offset = (uint4)((size_t)(tabent->value));
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
		dstlen += copy_length;
	}
	MV_INIT_STRING(dst, dstlen, stringpool.free);
	dst->mvtype |= MV_UTF_LEN; /* set character length since we know it */
	dst->str.char_len = char_len;
	stringpool.free = (unsigned char *)dstptr;
}

void op_fntranslate(mval *src, mval *srch, mval *rplc, mval *dst)
{
	static int xlate[NUM_CHARS];
	static hash_table_int4 *xlate_hash = NULL;
	static mstr prev_srch = {0, 0, NULL}, prev_rplc = {0, 0, NULL};
	static unsigned int prev_gcols = 0;

	MV_FORCE_STR(src);
	MV_FORCE_LEN(src); /* char_len needed for stringpool allocation */
	if (!badchar_inhibit)
	{	/* needed only to validate for BADCHARs */
		MV_FORCE_LEN(srch);
		MV_FORCE_LEN(rplc);
	}
	if (!((prev_gcols == stringpool.gcols) && (srch->str.addr == prev_srch.addr) && (srch->str.len == prev_srch.len)
			&& (rplc->str.addr == prev_rplc.addr) && (rplc->str.len == prev_rplc.len))
			|| ((NULL == prev_srch.addr) && (NULL == prev_rplc.addr)))	/* We need the last line of this if statement to
											 * avoid a sig-11 if srch and rplc are both undefined
											 * on the first $translate call */
	{
		if (NULL != xlate_hash)
		{
			free_hashtab_int4(xlate_hash);
			free(xlate_hash);
		}

		MV_FORCE_STR(srch);
		MV_FORCE_STR(rplc);
		/* If we had a static xlate_hash and prev_srch, prev_rplc, we could avoid this in rapid succession */
		xlate_hash = create_utf8_xlate_table(srch, rplc, xlate);
		prev_gcols = stringpool.gcols;
		prev_srch = srch->str;
		prev_rplc = rplc->str;
	}
	ENSURE_STP_FREE_SPACE(4 * src->str.len);
	op_fntranslate_common(src, dst, rplc, xlate, xlate_hash);
}

void op_fntranslate_fast(mval *src, mval *rplc, mval *m_xlate, mval *m_xlate_hash, mval *dst)
{
	hash_table_int4	*xlate_hash;  /* translation table to hold all multi-byte character mappings */
	int4 *xlate;

	MV_FORCE_STR(src);
	MV_FORCE_LEN(src); /* force BADCHAR if needed */
	ENSURE_STP_FREE_SPACE(4 * src->str.len); /* Allocate string space now so the stringpool doesn't shift xlate table */
	xlate = (int4 *)m_xlate->str.addr;
	assert(m_xlate->str.len == NUM_CHARS * SIZEOF(int4));
	if (0 != m_xlate_hash->str.len)
		xlate_hash = activate_hashtab_in_buffer_int4((sm_uc_ptr_t)m_xlate_hash->str.addr, NULL);
	else
		xlate_hash = NULL;
	op_fntranslate_common(src, dst, rplc, xlate, xlate_hash);
}
#endif /* UTF8_SUPPORTED */

void op_fnztranslate_common(mval *src, mval *dst, int *xlate)
{
	int4 n;
	unsigned char val;
	char *srcptr, *srctop, *dstptr;

	/* Callers are responsible for making sure there is space in the stringpool for the result */
	assert(IS_STP_SPACE_AVAILABLE(src->str.len));
	dstptr = (char *)stringpool.free;
	srcptr = src->str.addr;
	srctop = srcptr + src->str.len;

	while(srcptr < srctop)
	{
		val = *srcptr;
		n = xlate[val];
		if(DELETE_VALUE != n)
		{
			if (NO_VALUE == n)
				*dstptr = val;
			else
				*dstptr = n;
			dstptr++;
		}
		srcptr++;
	}
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST(dstptr - dst->str.addr);
	dst->mvtype = MV_STR;
	stringpool.free = (unsigned char*)dstptr;
}

/* $ZTRANSLATE() is implemented using a byte-indexed translation table xlate[NUM_CHARS] which stores the
 * replacement character (byte) for a given character (byte) of the second argument specified in $TR().
 */
void op_fnztranslate(mval *src, mval *srch, mval *rplc, mval *dst)
{
	static int xlate[NUM_CHARS];
	static mstr prev_srch = {0, 0, NULL}, prev_rplc = {0, 0, NULL};
	static unsigned int prev_gcols = 0;

	MV_FORCE_STR(src);
	if (!((prev_gcols == stringpool.gcols) && (srch->str.addr == prev_srch.addr) && (srch->str.len == prev_srch.len)
			&& (rplc->str.addr == prev_rplc.addr) && (rplc->str.len == prev_rplc.len))
			|| ((NULL == prev_srch.addr) && (NULL == prev_rplc.addr)))	/* We need the last line of this if statement to ensure
											 * the error message is produced if srch and rplc are
											 * are both undefined on the first $translate call */
	{
		MV_FORCE_STR(srch);
		MV_FORCE_STR(rplc);
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

	MV_FORCE_STR(src);
	ENSURE_STP_FREE_SPACE(src->str.len); /* Allocate string space now so the stringpool doesn't shift xlate table */
	xlate = (int4 *)m_xlate->str.addr;
	assert(m_xlate->str.len == NUM_CHARS * SIZEOF(int4));
	op_fnztranslate_common(src, dst, xlate);
}
