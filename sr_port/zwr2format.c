/****************************************************************
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_ctype.h"

#include "zshow.h"
#include "patcode.h"

#ifdef UTF8_SUPPORTED
#include "gtm_utf8.h"
#endif

#include "wbox_test_init.h"

LITREF	unsigned char	lower_to_upper_table[];

#define FORMAT_PRINTABLE(cp)					\
{								\
	memcpy(&dstptr[des->len], cpstart, (cp) - cpstart);	\
	des->len += (int)((cp) - cpstart);			\
}

#define FORMAT_CHAR(c)						\
{								\
	dstptr[des->len] = c;					\
	++(des->len);						\
}

/* Routine that transforms a ZWR subscript to the internal string representation */
boolean_t zwr2format(mstr *src, mstr *des)
{
	unsigned char 	ch, chtmp, *cp, *cpstart, *end, *dstptr, *strnext;
	int 		fastate, num;

	des->len = 0;
	if (src->len > 0)
	{
		cp = (unsigned char *)src->addr;
		end = cp + src->len;
		dstptr = (unsigned char*)des->addr;
		fastate = 0;
		for (cpstart = cp; cp < end; )
		{
			switch(fastate)
			{
			case 0: /* state that interprets graphic vs. non-graphic */
				ch = *cp++;
				if ('$' == ch)
				{	/* Check cp against end before each read so a truncated input cannot read past it */
					if (cp >= end)
						return FALSE;
					ch = chtmp = TOUPPER(*cp++);
					if (('C' != ch) && (('Z' != ch) || (cp >= end) || ('C' != (ch = TOUPPER(*cp++)))
							|| (cp >= end) || ('H' != (ch = TOUPPER(*cp++)))))
						return FALSE;
					if ((cp >= end) || ('(' != (ch = *cp++)))
						return FALSE;
					fastate = ('C' == chtmp) ? 2 : 3;
				} else if ('"' == ch)
				{ /* beginning of a quoted string: prepare for the new graphic substring */
					fastate = 1;
					cpstart = cp;
				} else if (('-' == ch) || (('0' <= ch) && (ch <= '9')))
				{ /* a numeric */
					FORMAT_CHAR(ch);
					fastate = 4;
				} else if ('.' == ch)
				{
					FORMAT_CHAR(ch);
					fastate = 5;
				} else
					return FALSE;
				break;
			case 1: /* Continuation of graphic string */
				ch = *cp++;
				if ('"' == ch)
				{
					if (cp < end)
					{
						switch (*cp)
						{
							case '"': /* print the graphic string upto the first quote */
								FORMAT_PRINTABLE(cp);
								cpstart = ++cp;
								break;
							case '_':
								FORMAT_PRINTABLE(cp - 1);
								fastate = 0;
								++cp;
								break;
							default:
								return FALSE;
						}
					} else
					{	/* closing quote is the last character of the input */
						FORMAT_PRINTABLE(cp - 1);
						fastate = 0;
					}
				}
				break;
			case 2:	/* parsing the string after $C( */
				A2I(cp, end, num);	/* NOTE: cp is updated accordingly */
				if (num < 0)
					return FALSE;
				if (!gtm_utf8_mode)
				{
					if (num > 255)
						return FALSE;
					FORMAT_CHAR(num);
				}
#				ifdef UTF8_SUPPORTED
				else
				{
					strnext = UTF8_WCTOMB(num, &dstptr[des->len]);
					if (strnext == &dstptr[des->len])
						return FALSE;	/* illegal code points in $C() */
					des->len += (int)(strnext - &dstptr[des->len]);
				}
#				endif
				if (cp >= end)
					return FALSE;	/* no ',' or ')' after the number, e.g. input ends with "$C(65" */
				switch(ch = *cp++)
				{
				case ',':
					break;
				case ')':
					if (cp < end && '_' != *cp++)
						return FALSE;
					fastate = 0;
					break;
				default:
					return FALSE;
					break;
				}
				break;
			case 3:	/* parsing the string after $ZCH( */
				A2I(cp, end, num);	/* NOTE: cp is updated accordingly */
				if (num < 0 || num > 255)
					return FALSE;
				FORMAT_CHAR(num);
				if (cp >= end)
					return FALSE;	/* no ',' or ')' after the number, e.g. input ends with "$ZCH(65" */
				switch(ch = *cp++)
				{
				case ',':
					break;
				case ')':
					if (cp < end && '_' != *cp++)
						return FALSE;
					fastate = 0;
					break;
				default:
					return FALSE;
					break;
				}
				break;
			case 4: /* a numeric - decimal might still come */
				ch = *cp++;
				if ('0' <= ch && ch <= '9')
				{
					FORMAT_CHAR(ch);
				} else if ('.' == ch)
				{
					FORMAT_CHAR(ch);
					fastate = 5;
				} else
					return FALSE;
				break;
			case 5: /* a numeric - already seen decimal */
				ch = *cp++;
				if ('0' <= ch && ch <= '9')
				{
					FORMAT_CHAR(ch);
				} else
					return FALSE;
				break;
			default:
				return FALSE;
				break;
			}
		}
		/* Accept only input ending after a complete string, $C()/$ZCH() or number */
		return ((fastate == 0) && (end[-1] != '_'))
			|| (((fastate == 4) || (fastate == 5)) && ('0' <= end[-1]) && (end[-1] <= '9'));
	}
	return TRUE;
}

/* Byte at offset "off" of "ptr", or '\0' if "off" is at or past "len" (the end of the input). No byte the key parser
 * looks for is '\0', so a truncated input ends the key instead of reading past the end of the input.
 */
#define	KEYCHAR(ptr, off, len)	(((off) < (len)) ? (ptr)[off] : '\0')

/* Routine to compute the length of the KEY, length of the VALUE and Offset of VALUE in ZWR format.
 * This function takes 2 inputs and returns 5 piece of information.
 * INPUT:
 *	ptr		: starting address of the zwr format string.
 *	len		: length of the zwr format string.
 * OUTPUT:
 *	lenght of the key
 *	val_off		: offset of the value(Right hand side of the '=' sign).
 *	val_len		: length of the value(Length of the value present on right hand side of '=' sign).
 *	val_off1	: offset inside the value of spanning node present.
 *	val_len1	: length of data (of spanning node) present in the block.
 * Only a caller that passes non-NULL val_off1 and val_len1 accepts the $ze(...) format. For any other caller, a
 * leading '$' is parsed as part of an ordinary key. A $ze(...) format input with no closing ')' returns a negative
 * val_len.
 */
int zwrkeyvallen(char* ptr, int len, char **val_off, int *val_len, int *val_off1, int *val_len1)
{
	int		keylength, keystate, off, *tmp;
	unsigned	ch, chtmp;
	boolean_t	keepgoing, extfmt;

	keylength = 0;	/* determine length of key */
	keystate  = 0;
	extfmt = (NULL != val_off1) && (NULL != val_len1) && (0 < len) && ('$' == ptr[0]);
	if (extfmt)
	{	/* In extract first 4 chars are '$', 'z', 'e' and '(' */
		ptr += 4;
		len = (4 < len) ? (len - 4) : 0;
	}
	keepgoing = TRUE;
	while ((keylength < len) && keepgoing) /* slightly different here from go_load since we can get kill records too */
	{
		ch = ptr[keylength++];
		switch (keystate)
		{
		case 0:	/* in global name */
			if ('=' == ch)	/* end of key */
			{
				keylength--;
				keepgoing = FALSE;
			} else if (',' == ch)
			{
				keylength--;
				keepgoing = FALSE;
			} else if ('(' == ch) /* start of subscripts */
				keystate = 1;
			break;
		case 1:	/* in subscripts area, but out of "..." or $C(...) */
			switch (ch)
			{
			case ')': /* end of subscripts ==> end of key */
				keepgoing = FALSE;
				break;
			case '"': /* step into "..." */
				keystate = 2;
				break;
			case '$': /* step into $C(...) */
				chtmp = TOUPPER(KEYCHAR(ptr, keylength, len));
				keylength += ('C' == chtmp) ? 2 : 4;
				keystate = 3;
				break;
			}
			break;
		case 2:	/* in "..." */
			if ('"' == ch)
			{
				switch (KEYCHAR(ptr, keylength, len))
				{
				case '"': /* "" */
					keylength++;
					break;
				case '_': /* _$C(...) or _$ZCH(...) */
					chtmp = TOUPPER(KEYCHAR(ptr, keylength + 2, len));
					keylength += ('C' == chtmp) ? 4 : 6;
					keystate = 3;
					break;
				default: /* step out of "..." */
					keystate = 1;
				}
			}
			break;
		case 3:	/* in $C(...) or $ZCH(...) */
			if (')' == ch)
			{
				if ('_' == KEYCHAR(ptr, keylength, len)) /* step into "..." or $C(...) or $ZCH(...) */
				{
					if ('"' == KEYCHAR(ptr, keylength + 1, len))
					{ /* step into "..." by advancing over the begin quote */
						keylength += 2;
						keystate = 2;
					} else
					{ /* step into $C(..) or $ZCH(...) but do not advance over dollar ($) */
						keylength += 1;
						keystate = 1;
					}
					break;
				} else
					keystate = 1; /* step out of $C(...) */
			}
			break;
		default:
			assert(FALSE);
			break;
		}
	}
	if (len < keylength)
		keylength = len;	/* a skip over "$C(" and the like went past the end of a truncated input */
	if (extfmt)
	{
		off = keylength + 1;	/* to point to second exp in $ext format */
		tmp = val_off1;
		*tmp = 0;
		*val_len1 = 0;
		ch = '\0';
		while (off < len)
		{
			ch = ptr[off++];
			if (')' == ch)
				break;
			if (',' == ch)
			{
				tmp = val_len1;
				*tmp = 0;
				continue;
			}
			*tmp = (10 * (*tmp)) + ch - 48;
		}
		if (')' != ch)
		{	/* the input ended before the closing ')' */
			*val_off = ptr + off;
			*val_len = -1;
		} else
		{
			*val_off = ptr + off + SIZEOF(char);	/* SIZEOF(char) is used to make adjustment for '=' sign */
			*val_len = len - (off + SIZEOF(char));	/* len already excludes the prefix '$ze(' */
		}
	} else
	{
		*val_off = ptr + (keylength + SIZEOF(char));	/* SIZEOF(char) is used to make adjustment for '=' sign */
		*val_len = len - (keylength + SIZEOF(char)); 	/* SIZEOF(char) is used to make adjustment for '=' sign */
	}
	return keylength;
}
