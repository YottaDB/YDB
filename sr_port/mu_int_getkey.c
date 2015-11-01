/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtm_ctype.h"
#include "util.h"
#include "mupint.h"
#include "mvalconv.h"

GBLDEF	gv_key		*muint_start_key;
GBLDEF	gv_key		*muint_end_key;
GBLDEF	int		muint_start_keyend;
GBLDEF	int		muint_end_keyend;

GBLREF  boolean_t	muint_key;
GBLREF	boolean_t	muint_subsc;

int mu_int_getkey(unsigned char *key_buff, int keylen)
{
	mval				v;
	unsigned char			*ptr, *top, *src, *dest, slit[MAX_KEY_SZ + 1], *tmp;

	src = key_buff;
	if ('"' == key_buff[keylen - 1])
		keylen--;
	if ('"' == key_buff[0])
	{
		keylen--;
		src++;
	}
	muint_start_key = (gv_key*)malloc(sizeof(gv_key) + MAX_KEY_SZ + 1);
	muint_start_key->top = MAX_KEY_SZ + 1;
	muint_start_key->prev = 0;
	for (ptr = src, top = ptr + keylen; ptr < top; ptr++)
	{
		if ((':' == *ptr) && ('^' == *(ptr + 1)))
			break;
	}
	if ('^' != *src++)
	{
		util_out_print("Error: invalid key.", TRUE);
		return FALSE;
	}
	if ((MAX_KEY_SZ < (ptr - src)) || (MAX_KEY_SZ < (top - ptr)))
	{
		util_out_print("Error: key too long.", TRUE);
		return FALSE;
	}
	dest = muint_start_key->base;
	if (ISALPHA(*src) || ('%' == *src))
		*dest++ = *src++;
	else
	{
		util_out_print("Error: invalid key.", TRUE);
		return FALSE;
	}
	for ( ; (':' != *src) && ('(' != *src) && src < ptr ;src++)
	{
		if (ISALNUM(*src))
			*dest = *src;
		else
		{
			util_out_print("Error: invalid key.", TRUE);
			return FALSE;
		}
		dest++;
	}
	*dest++ = 0;
	*dest = 0;
	muint_start_key->end = dest - muint_start_key->base;
	muint_start_keyend = muint_start_key->end;
	if ('(' == *src)
	{
		muint_subsc = TRUE;
		src++;
		for (;;)
		{
			v.mvtype = MV_STR;
			if ('\"' != *src)
			{
				for (v.str.addr = (char*)src ; (')' != *src) && (',' != *src) ; src++)
				{
					if (src == ptr || (*src < '0' || *src > '9') && ('-' != *src) && ('.' != *src))
					{
						util_out_print("Error: invalid key.", TRUE);
						return FALSE;
					}
				}
				v.str.len = src - (unsigned char*)v.str.addr;
				s2n(&v);
				v.mvtype &= MV_NUM_MASK;
			} else
			{
				src++;
				tmp = slit;
				for (;;)
				{
					if (src == ptr)
					{
						util_out_print("Error: invalid key.", TRUE);
						return FALSE;
					}
					if ('\"' == *src)
						if ('\"' != *++src)
							break;
					*tmp++ = *src++;
				}
				v.str.addr = (char*)slit;
				v.str.len = tmp - slit;
			}
			mval2subsc(&v, muint_start_key);
			if (',' != *src)
				break;
			src++;
		}
		if (')' != *src++)
		{
			util_out_print("Error: invalid key.", TRUE);
			return FALSE;
		}
		dest = muint_start_key->base + muint_start_key->end;
	}
	muint_key = TRUE;
	if (ptr == top)
		return TRUE;
	muint_end_key = (gv_key*)malloc(MAX_KEY_SZ + 1);
	muint_end_key->top = MAX_KEY_SZ + 1;
	muint_end_key->prev = 0;
	ptr++;
	src = ptr;
	dest = muint_end_key->base;
	if ('^' != *src++)
	{
		util_out_print("Error: invalid key.", TRUE);
		return FALSE;
	}
	if (ISALPHA(*src) || ('%' == *src))
		*dest++ = *src++;
	else
	{
		util_out_print("Error: invalid key.", TRUE);
		return FALSE;
	}
	for ( ; ('(' != *src) && src < top ;src++)
	{
		if (ISALNUM(*src))
			*dest = *src;
		else
		{
			util_out_print("Error: invalid key.", TRUE);
			return FALSE;
		}
		dest++;
	}
	*dest++ = 0;
	*dest = 0;
	muint_end_key->end = dest - muint_end_key->base;
	muint_end_keyend = muint_end_key->end;
	if ('(' == *src)
	{
		muint_subsc = TRUE;
		src++;
		for (;;)
		{
			v.mvtype = MV_STR;
			if ('\"' != *src)
			{
				for (v.str.addr = (char*)src ; (')' != *src) && (',' != *src) ; src++)
				{
					if (src == top || (*src < '0' || *src > '9') && ('-' != *src) && ('.' != *src))
					{
						util_out_print("Error: invalid key.", TRUE);
						return FALSE;
					}
				}
				v.str.len = src - (unsigned char *)v.str.addr;
				s2n(&v);
				v.mvtype &= MV_NUM_MASK;
			} else
			{
				src++;
				tmp = slit;
				for (;;)
				{
					if (src == top)
					{
						util_out_print("Error: invalid key.", TRUE);
						return FALSE;
					}
					if ('\"' == *src)
						if ('\"' != *++src)
							break;
					*tmp++ = *src++;
				}
				v.str.addr = (char *)slit;
				v.str.len = tmp - slit;
			}
			mval2subsc(&v, muint_end_key);
			if (',' != *src)
				break;
			src++;
		}
		if (')' != *src++)
		{
			util_out_print("Error: invalid key.", TRUE);
			return FALSE;
		}
		dest = muint_end_key->base + muint_end_key->end;
	}
	return TRUE;
}
