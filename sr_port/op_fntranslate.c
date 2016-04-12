/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "op.h"

GBLREF spdesc stringpool;

#ifdef UNICODE_SUPPORTED
#include "hashtab_int4.h"
#include "hashtab.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	badchar_inhibit;

error_def(ERR_MAXSTRLEN);

/******************************************************************************
 $TRANSLATE() - Multi-byte character translation.

  This algorithm is similar to $ZTRANSLATE() but uses a hash table (hash_table_int4)
  to store multi-byte character mappings in addition to the xlate[] array used
  for single-byte characters.  The key differences are:

  Datastructures:
	The stringpool buffer is computed based on keeping track of additional
	space needed for every input->output character translation in addition
	to the source length (src->str.len).

	Uses xlate[] an array of 'int4' rather than 'short int' to store the
	(1-based) byte offset of the replacement character within out_str rather than the
	character itself.

	Since "stp_gcol" can potentially change out_str address, the byte offsets
	are stored instead of byte pointers. Also, note that the offset is not
	0-based, since 0 can not be stored as valid entry in hash table.

  Algorithm:
	If the character being indexed is single-byte (ASCII or illegal byte),
	store the offset to the corresponding replacement out_str character
	in the xlate[] element. If the character being indexed is multi-byte,
	store the offset to the replacing out_str character in the hash table
	using the code point as the hash key.

	For the input characters that do not have corresponding replacing
	ouput characters, store MAXPOSINT4 in the respective table.

	Iterate over each character of the source string and perform the
	translation according to the mapping specified above.

  Note:
  	If input is entirely of ASCII characters, this algorithm will not use
	the hash table, therefore making its efficiency closer to that of the
	byte-oriented algorithm.
******************************************************************************/

void op_fntranslate(mval *src, mval *in_str, mval *out_str, mval *dst)
{
	unsigned char	*inptr, *intop, *outptr, *outbase, *outtop, *dstbase, *nextptr, *chptr;
	int4		xlate[256]; /* translation table to hold all single-byte character mappings */
	hash_table_int4	xlate_hash;  /* translation table to hold all multi-byte character mappings */
	INTPTR_T	choff;	/* byte offset of the character within out_str */
	ht_ent_int4	*tabent;
	unsigned char 	ch, drop;
	int 		n, max_len_incr, size, inlen, outlen, char_len, chlen, dstlen;
	uint4		code;
	boolean_t	hashtab_created, char_already_seen;

	MV_FORCE_STR(src);
	MV_FORCE_STR(in_str);
	MV_FORCE_STR(out_str);

	MV_FORCE_LEN(src); /* char_len needed for stringpool allocation */
	if (!badchar_inhibit)
	{	/* needed only to validate for BADCHARs */
		MV_FORCE_LEN(in_str);
		MV_FORCE_LEN(out_str);
	}
	/* Initialize both translation tables & other stuff */
	memset(xlate, 0, SIZEOF(xlate));
	if (!MV_IS_SINGLEBYTE(in_str))
	{ /* hash table not needed if input is entirely single byte */
		init_hashtab_int4(&xlate_hash, 0, HASHTAB_COMPACT, HASHTAB_SPARE_TABLE);
		hashtab_created = TRUE;
	} else
		hashtab_created = FALSE;
	max_len_incr = 0;
	/* Initialize the translation table with the mapping */
	inptr = (unsigned char *)in_str->str.addr;
	outbase = outptr = (unsigned char *)out_str->str.addr;
	intop = inptr + in_str->str.len;
	outtop = outptr + out_str->str.len;
	for (char_len = 0; inptr < intop && outptr < outtop; ++char_len, inptr = nextptr, outptr += outlen)
	{
		nextptr = UTF8_MBTOWC(inptr, intop, code);
		inlen = (int)(nextptr - inptr);
		assert(inlen > 0);
		assert((1 == inlen) || (WEOF != code));
		assert((1 == inlen) || hashtab_created);
		if (1 == inlen)
		{
			if (0 == xlate[*inptr]) /* store 1-based byte offset */
			{
		    		xlate[*inptr] = (int)(outptr - outbase + 1);
				char_already_seen = FALSE;
			} else
				char_already_seen = TRUE;
		} else if (!lookup_hashtab_int4(&xlate_hash, &code))
		{
			add_hashtab_int4(&xlate_hash, &code, (void*)(outptr - outbase + 1), &tabent);
			char_already_seen = FALSE;
		} else
			char_already_seen = TRUE;
		outlen =(int)(UTF8_MBNEXT(outptr, outtop) - outptr); /* byte length of replacement character */
		if (!char_already_seen && (n = outlen - inlen) > max_len_incr)
			max_len_incr = n; /* extra stringpool space needed if this translation occurs */
	}
	/* Mark those input characters that do not have translation */
	for (; inptr < intop; inptr = nextptr, ++char_len)
	{
		nextptr = UTF8_MBTOWC(inptr, intop, code);
		inlen = (int)(nextptr - inptr);
		assert(inlen > 0);
		assert((1 == inlen) || (WEOF != code));
		assert((1 == inlen) || hashtab_created);
		if (1 == inlen)
		{
			if (0 == xlate[*inptr])
		    		xlate[*inptr] = MAXPOSINT4;
		} else if (!lookup_hashtab_int4(&xlate_hash, &code))
			add_hashtab_int4(&xlate_hash, &code, (void*)MAXPOSINT4, &tabent);
	}
	assert(!(in_str->mvtype & MV_UTF_LEN) || in_str->str.char_len == char_len);
	if (!(in_str->mvtype & MV_UTF_LEN))
	{	/* now that we've processed in_str entirely, set its char_len since we know it */
		in_str->mvtype |= MV_UTF_LEN;
		in_str->str.char_len = char_len;
	}
	/* The result string size can potentially increase by the factor of max_len_incr, which is the maximum increase
	 * of byte length amongst all character mapppings from in_str to out_str. This may be an over-allocation, but
	 * that's the most conservative size guaranteed to hold the result.
	 */
	size = src->str.len + src->str.char_len * max_len_incr;
	size = (size > MAX_STRLEN) ? MAX_STRLEN : size;
	ENSURE_STP_FREE_SPACE(size);
	outbase = (unsigned char *)out_str->str.addr; /* recompute in case "stp_gcol" changes out_str->str.addr */
	outtop = outbase + out_str->str.len;
	dstbase = stringpool.free;
	dstlen = char_len = 0; /* character length of the result */
	for (inptr = (unsigned char *)src->str.addr, intop = inptr + src->str.len; inptr < intop; inptr = nextptr)
	{
		nextptr = UTF8_MBTOWC(inptr, intop, code);
		inlen = (int)(nextptr - inptr);
		assert(inlen > 0);
		if (1 == inlen)
			choff = xlate[*inptr];
		else {
			tabent = hashtab_created ? (ht_ent_int4*)lookup_hashtab_int4(&xlate_hash, &code): NULL;
			choff = (NULL != tabent) ? (INTPTR_T)tabent->value : 0;
		}
		if (0 == choff)
		{ /* no translation exists, retain the source character */
			chptr = inptr;
			chlen = inlen;
		} else if (MAXPOSINT4 != choff)
		{ /* translation exists, replace the source character */
			assert(choff > 0);
			chptr = &outbase[choff - 1];	/* retreive the character using 1-based index */
			chlen = (int)(UTF8_MBNEXT(chptr, outtop) - chptr);
			assert(chlen > 0);
		}
		if (MAXPOSINT4 != choff)
		{ /* add a new character into the result based on the translation above */
			if (dstlen + chlen > MAX_STRLEN)
			 	rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
			if (1 == chlen)
				dstbase[dstlen] = *chptr;
			else
				memcpy(&dstbase[dstlen], chptr, chlen);
			dstlen += chlen;
			++char_len;
		}
	}
	if (hashtab_created)
		free_hashtab_int4(&xlate_hash);
	MV_INIT_STRING(dst, dstlen, (char *)dstbase);
	assert(dst->str.len <= size);
	dst->mvtype |= MV_UTF_LEN; /* set character length since we know it */
	dst->str.char_len = char_len;
	stringpool.free = dstbase + dstlen;
}
#endif /* UNICODE_SUPPORTED */

/* $ZTRANSLATE() is implemented using a byte-indexed translation table xlate[256] which stores the
 * replacement character (byte) for a given character (byte) of the second argument specified in $TR().
 */
void op_fnztranslate(mval *src, mval *in_str, mval *out_str, mval *dst)
{
	int		n, xlate[256];
	unsigned char	ch, *inpt, *intop, *outpt, *dstp;

	MV_FORCE_STR(src);
	MV_FORCE_STR(in_str);
	MV_FORCE_STR(out_str);
	ENSURE_STP_FREE_SPACE(src->str.len);
	memset(xlate, 0xFF, SIZEOF(xlate));
	n = in_str->str.len < out_str->str.len ? in_str->str.len : out_str->str.len;
	for (inpt = (unsigned char *)in_str->str.addr, outpt = (unsigned char *)out_str->str.addr, intop = inpt + n;
		inpt < intop; inpt++, outpt++)
	{
		if (-1 == xlate[ch = *inpt])
		    xlate[ch] = *outpt;
	}
	for (intop = (unsigned char *)in_str->str.addr + in_str->str.len; inpt < intop; inpt++)
		if (-1 == xlate[ch = *inpt])
		    xlate[ch] = -2;
	dstp = outpt = stringpool.free;
	for (inpt = (unsigned char *)src->str.addr, intop = inpt + src->str.len; inpt < intop; )
	{
		n = xlate[ch = *inpt++];
		if (0 <= n)
			*outpt++ = n;
		else if (-1 == n)
			*outpt++ = ch;
	}
	dst->str.addr = (char *)dstp;
	dst->str.len = INTCAST(outpt - dstp);
	dst->mvtype = MV_STR;
	stringpool.free = outpt;
}
