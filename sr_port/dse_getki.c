/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_ctype.h"
#include "gtm_facility.h"
#include "gtm_stdlib.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "min_max.h"          /* needed for init_root_gv.h */
#include "init_root_gv.h"
#include "util.h"
#include "cli.h"
#include "stringpool.h"
#include "dse.h"
#include "mvalconv.h"
#include "op.h"
#include "format_targ_key.h"

GBLREF gv_key		*gv_currkey;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF gd_region    	*gv_cur_region;
GBLREF mval		curr_gbl_root;

int dse_getki(char *dst, int *len, char *qual, int qual_len)
{
	char 		buf[MAX_ZWR_KEY_SZ], *src, *temp_dst, *bot, *top, *tmp, slit[MAX_KEY_SZ + 1], key_buf[MAX_KEY_SZ + 1];
	char		*end;
	short int	max_key;
	unsigned short 	buf_len;
	int  		key_len, dlr_num, dlr_len, parse_dlr_char(char *, char *, char *);
	mval 		key_subsc;

	error_def(ERR_GVSUBOFLOW);
	error_def(ERR_GVIS);

	buf_len = sizeof(buf);
	if (!cli_get_str(qual, buf, &buf_len))
		return FALSE;
	bot = temp_dst = (char *)&key_buf[0];
	top = &buf[buf_len];
	src = &buf[0];
	if (*src++ != '^')
	{
		util_out_print("Error:  invalid key.", TRUE);
		return FALSE;
	}
	if ((*src >= 'A' && *src <= 'Z') ||
	    (*src >= 'a' && *src <= 'z') ||
	    (*src == '%'))					/* first letter must be an alphabet or % */
	{
		*temp_dst++ = *src++;
	} else
	{
		util_out_print("Error:  invalid key.", TRUE);
		return FALSE;
	}
	for ( ; *src != '(' && src < top ;src++)
	{
		if ((*src >= 'A' && *src <= 'Z') ||
		    (*src >= 'a' && *src <= 'z') ||
		    (*src >= '0' && *src <= '9'))
			*temp_dst = *src;
		else
		{
			util_out_print("Error:  invalid key.", TRUE);
			return FALSE;
		}
		temp_dst++;
	}
	*temp_dst = '\0';

	key_len = (int )(temp_dst - bot);
	INIT_ROOT_GVT(bot, key_len, curr_gbl_root);
	bot = (char *)&gv_currkey->base[0];
	temp_dst = (char *)&gv_currkey->base[0] + gv_currkey->end;
	max_key = gv_cur_region->max_key_size;
	if (*src == '(')
	{
		src++;
		for (;;)
		{
			key_subsc.mvtype = MV_STR;
			if (*src == '$')			/* may be a $char() */
			{
				src++;
				if ((dlr_len = parse_dlr_char(src, top, slit)) > 0)
				{
					key_subsc.str.addr = slit;
					key_subsc.str.len = strlen(slit);
					src += dlr_len;
				} else
				{
					util_out_print("Error:  invalid key.", TRUE);
					return FALSE;
				}
			} else if (*src != '\"')		/* numerical subscript */
			{
				for (key_subsc.str.addr = src ; *src != ')' && *src != ','; src++)
					if (src == top || (*src < '0' || *src > '9') && *src != '-' && *src != '.')
					{
						util_out_print("Error:  invalid key.", TRUE);
						return FALSE;
					}
				key_subsc.str.len = src - key_subsc.str.addr;
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
					if (*src == '\"')
						if (*++src != '\"')
							break;
					*tmp++ = *src++;
				}
				key_subsc.str.addr = slit;
				key_subsc.str.len = tmp - slit;
			}

			if (!(cs_addrs->hdr->null_subs || key_subsc.str.len))
			{
				util_out_print("Error:  Null subscripts not allowed", TRUE);
				return FALSE;
		        }

			mval2subsc(&key_subsc, gv_currkey);
			if (gv_currkey->end >= max_key)
			{
				if (0 == (end = (char *)format_targ_key((uchar_ptr_t)buf, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
					end = &buf[MAX_ZWR_KEY_SZ - 1];
				rts_error(VARLSTCNT(6) ERR_GVSUBOFLOW, 0, ERR_GVIS, 2, end - buf, buf);
			}
			if (*src != ',')
				break;
			src++;
		}
		if (*src++ != ')')
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
	*len = temp_dst - bot + 1;
	memcpy(dst, &gv_currkey->base[0], *len);
	return TRUE;
}

int parse_dlr_char(char *src, char *top, char *dlr_subsc)
{
	int		indx = 0, dlr_len, dlr_val, harlen;
	char 		lcl_buf[MAX_KEY_SZ + 1];
	char 		*tmp_buf;

	tmp_buf = src;

	if (*tmp_buf != 'c' && *tmp_buf != 'C')
		return 0;
	tmp_buf++;
	if (*tmp_buf == 'h' || *tmp_buf == 'H')
	{
		harlen = strlen("har");
		if (top - tmp_buf <= harlen)
			return 0;
		if (STRNCASECMP(tmp_buf, "har", harlen))
			return 0;
		tmp_buf += harlen;
	}
	if (*tmp_buf++ != '(')
		return 0;
	if (!ISDIGIT(*tmp_buf))
		return 0;
	while (tmp_buf != top)
	{
		if (ISDIGIT(*tmp_buf))
			lcl_buf[indx++] = *tmp_buf;
		else if (',' == *tmp_buf || ')' == *tmp_buf)
		{
			lcl_buf[indx] = '\0';
			dlr_val = ATOI(lcl_buf);
			dlr_val = (dlr_val > 255) ? 255 : dlr_val;
			*dlr_subsc++ = (unsigned char)dlr_val;
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
	return (tmp_buf - src);
}
