/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_strings.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_ctype.h"
#include "gtm_facility.h"
#include "gtm_stdlib.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "util.h"
#include "cli.h"
#include "stringpool.h"
#include "dse.h"
#include "mvalconv.h"
#include "op.h"
#include "format_targ_key.h"
#include "gvcst_protos.h"

#ifdef GTM_TRIGGER
#include "hashtab_mname.h"
#include <rtnhdr.h>
#include "targ_alloc.h"
#endif
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

GBLREF	gv_key		*gv_currkey;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_namehead	*gv_target;

int dse_getki(char *dst, int *len, char *qual, int qual_len)
{
	char 		buf[MAX_ZWR_KEY_SZ], *src, *temp_dst, *bot, *top, *tmp, slit[MAX_KEY_SZ + 1], key_buf[MAX_KEY_SZ + 1];
	short int	max_key;
	unsigned short 	buf_len;
	int  		dlr_num, dlr_len;
	int		num;
	unsigned char	*ptr;
	mval 		key_subsc;
	span_subs	subs;

	buf_len = SIZEOF(buf);
	if (!cli_get_str(qual, buf, &buf_len))
		return FALSE;
	bot = temp_dst = (char *)&key_buf[0];
	top = &buf[buf_len];
	src = &buf[0];
	if ('^' != *src++)
	{
		util_out_print("Error:  invalid key.", TRUE);
		return FALSE;
	}
	if ((('A' <= *src) && ('Z' >= *src))
		|| (('a' <= *src) && ('z' >= *src))
		|| ('%' == *src) || ('#' == *src))			/* first letter must be an alphabet or % or # */
	{
		*temp_dst++ = *src++;
	} else
	{
		util_out_print("Error:  invalid key.", TRUE);
		return FALSE;
	}
	for ( ; ('(' != *src) && (src < top); src++)
	{
		if ((('A' <= *src) && ('Z' >= *src))
			|| (('a' <= *src) && ('z' >= *src))
			|| (('0' <= *src) && ('9' >= *src)))
		{
			*temp_dst = *src;
		} else
		{
			util_out_print("Error:  invalid key.", TRUE);
			return FALSE;
		}
		temp_dst++;
	}
	*temp_dst = '\0';
	gv_target = dse_find_gvt(gv_cur_region, bot, (int)(temp_dst - bot));
	SET_GV_CURRKEY_FROM_GVT(gv_target);	/* gv_currkey is needed by GVCST_ROOT_SEARCH AND code after it */
	GVCST_ROOT_SEARCH; /* sets up gv_target->collseq (needed by mval2subsc below ) */
	bot = (char *)&gv_currkey->base[0];
	temp_dst = (char *)&gv_currkey->base[0] + gv_currkey->end;
	max_key = gv_cur_region->max_key_size;
	if ('(' == *src)
	{
		src++;
		for (;;)
		{
			key_subsc.mvtype = MV_STR;
			if ('$' == *src)			/* may be a $char() */
			{
				src++;
				if ((dlr_len = parse_dlr_char(src, top, slit)) > 0)
				{
					key_subsc.str.addr = slit;
					key_subsc.str.len = STRLEN(slit);
					src += dlr_len;
				} else
				{
					util_out_print("Error:  invalid key.", TRUE);
					return FALSE;
				}
			} else if ('#' == *src)
			{	/*Special spanning global subscript*/
				if ('S' != TOUPPER(*(src + 1)) && 'P' != TOUPPER(*(src + 2))
				     && 'A' != TOUPPER(*(src + 3)) && 'N' != TOUPPER(*(src + 4)))
				{
					util_out_print("Error:  invalid key.", TRUE);
					return FALSE;
				}
				src = src + SPAN_SUBS_LEN + 1;
				for (num = 0, src++; ')' != *src; num = (num * DECIMAL_BASE + (int)(*src++ - ASCII_0)))
					;
				ptr = gv_currkey->base + gv_currkey->end;
				num = num - 1;
				SPAN_INITSUBS(&subs, num);
				SPAN_SUBSCOPY_SRC2DST(ptr, (unsigned char *)&subs);
				ptr = ptr + SPAN_SUBS_LEN;
				*ptr++ = KEY_DELIMITER;
				*ptr = KEY_DELIMITER;
				gv_currkey->end = ptr - gv_currkey->base;
				break;
			} else if ('\"' != *src)		/* numerical subscript */
			{
				for (key_subsc.str.addr = src ; (')' != *src) && (',' != *src); src++)
				{
					if (src == top || (('0' > *src) || ('9' < *src)) && ('-' != *src) && ('.' != *src))
					{
						util_out_print("Error:  invalid key.", TRUE);
						return FALSE;
					}
				}
				key_subsc.str.len = INTCAST(src - key_subsc.str.addr);
				s2n(&key_subsc);
				key_subsc.mvtype &= MV_NUM_MASK;
			} else
			{
				src++;
				tmp = slit;
				for (;;)
				{
					if (src == top)
					{
						util_out_print("Error:  invalid key.", TRUE);
						return FALSE;
					}
					if ('\"' == *src)
						if ('\"' != *++src)
							break;
					*tmp++ = *src++;
				}
				key_subsc.str.addr = slit;
				key_subsc.str.len = INTCAST(tmp - slit);
			}
			if ( 0 == key_subsc.str.len && NEVER == cs_addrs->hdr->null_subs)
			{
				util_out_print("Error:  Null subscripts not allowed", TRUE);
				return FALSE;
		        }
			mval2subsc(&key_subsc, gv_currkey, gv_cur_region->std_null_coll);
			if (gv_currkey->end >= max_key)
				ISSUE_GVSUBOFLOW_ERROR(gv_currkey, KEY_COMPLETE_TRUE);
			if (',' != *src)
				break;
			src++;
		}
		if (')' != *src++)
		{
			util_out_print("Error:  invalid key.", TRUE);
			return FALSE;
		}
		temp_dst = (char *)&gv_currkey->base[0] + gv_currkey->end;
	}
	if (src != top)
	{
		util_out_print("Error:  invalid key.", TRUE);
		return FALSE;
	}
	*len = (int)(temp_dst - bot + 1);
	assert(3 <= *len);
	assert(KEY_DELIMITER != gv_currkey->base[*len - 3]);
	assert(KEY_DELIMITER == gv_currkey->base[*len - 2]);
	assert(KEY_DELIMITER == gv_currkey->base[*len - 1]);
	memcpy(dst, &gv_currkey->base[0], *len);
	return TRUE;
}

int parse_dlr_char(char *src, char *top, char *dlr_subsc)
{
	int		indx = 0, dlr_len, dlr_val, harlen;
	char 		lcl_buf[MAX_KEY_SZ + 1];
	char 		*tmp_buf, *strnext;
	boolean_t	dlrzchar = FALSE;

	tmp_buf = src;
	if ('Z' == TOUPPER(*tmp_buf))
	{
		dlrzchar = TRUE;
		tmp_buf++;
	}
	if ('C' != TOUPPER(*tmp_buf++))
		return 0;
	if ('H' == TOUPPER(*tmp_buf))
	{
		if (top - tmp_buf <= STR_LIT_LEN("har") || STRNCASECMP(tmp_buf, "har", STR_LIT_LEN("har")))
		{
			if (!dlrzchar)
				return 0;
			tmp_buf++;
		}
		else
			tmp_buf += STR_LIT_LEN("har");
	} else if (dlrzchar)
		return 0;
	if (*tmp_buf++ != '(')
		return 0;
	if (!ISDIGIT_ASCII(*tmp_buf))
		return 0;
	while (tmp_buf != top)
	{
		if (ISDIGIT_ASCII(*tmp_buf))
			lcl_buf[indx++] = *tmp_buf;
		else if (',' == *tmp_buf || ')' == *tmp_buf)
		{
			lcl_buf[indx] = '\0';
			dlr_val = ATOI(lcl_buf);
			if (0 > dlr_val)
				return 0;
			if (!gtm_utf8_mode || dlrzchar)
			{
				if (255 < dlr_val)
					return 0;
				*dlr_subsc++ = dlr_val;
			}
#			ifdef UNICODE_SUPPORTED
			else {
				strnext = (char *)UTF8_WCTOMB(dlr_val, dlr_subsc);
				if (strnext == dlr_subsc)
					return 0;
				dlr_subsc = strnext;
			}
#			endif
			indx = 0;
			if (')' == *tmp_buf)
			{
				*dlr_subsc = '\0';
				break;
			}
		} else
			return 0;
		tmp_buf++;
	}
	if (tmp_buf == top)
		return 0;
	tmp_buf++;
	return (int)(tmp_buf - src);
}
