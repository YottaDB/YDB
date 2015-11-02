/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gtmctype.h"
#include "util.h"
#include "gvsub2str.h"
#include "print_target.h"

#ifdef UNICODE_SUPPORTED
#include "gtm_icu_api.h"
#include "gtm_utf8.h"
#endif

GBLREF gd_region       *gv_cur_region;
LITDEF char 		spaces[] = "    ";

void print_target(unsigned char *c)
{
	unsigned char	ctemp, *p, *ptop, *ptop1, *ptr, *ptr1, *top, *p_next;
	unsigned char	buff[MAX_ZWR_KEY_SZ + SIZEOF("?.0")];
	uint4		ch;
	boolean_t	bad_sub = FALSE;
	boolean_t	is_string;

	ptop = c + MAX_KEY_SZ;
	for (p = buff, ptr = c;  *ptr && (ptr < ptop);  ptr++)
	{
		if (PRINTABLE(*ptr))
			*p++ = *ptr;
		else
			*p++ = '.';
	}
	*p = 0;
	util_out_print("!AD", FALSE, p - buff, buff);
	if (ptr >= ptop)
	{
		util_out_print("...", FALSE);
		return;
	}
	ptr++;
	if (!*ptr)
		return;
	util_out_print("(", FALSE);
	for (;;)
	{
		if (STR_SUB_PREFIX == *ptr || SUBSCRIPT_STDCOL_NULL == *ptr)
		{
			is_string = TRUE;
			ptop1 = ptop;
			util_out_print("\"", FALSE);
		} else
		{
			is_string = FALSE;
			ptop1 = ptr + MAX_GVKEY_PADDING_LEN;
		}
		for (ptr1 = ptr;  *ptr1;  ptr1++)
		{
			assert(ptr1 <= ptop1);
			if (ptr1 >= ptop1)
			{
				bad_sub = TRUE;
				ptr1--;
				ctemp = *ptr1;
			 	*ptr1 = 0;
				break;
			}
		}
		top = gvsub2str(ptr, buff, FALSE);
		if (!is_string && (0x80 != *ptr++) && (KEY_DELIMITER == *ptr))
		{
			top = (unsigned char *)(buff + SIZEOF("?.0"));	/* to allow a bit of garbage, in case it's helpful */
			*top++ = '*';					/* to keep the garbage short and identified as garbage */
		}
		*top = 0;
		if (!gtm_utf8_mode)
		{
			for (p = buff;  p < top;  ++p)
				if (!PRINTABLE(*p))
					*p = '.';
		}
#ifdef UNICODE_SUPPORTED
		else {
			for (p = buff;  p < top;  p = p_next)
			{
				p_next = UTF8_MBTOWC(p, top, ch);
				if (WEOF == ch || !U_ISPRINT(ch))
				{ /* non-printable or illegal characters */
					for (; p < p_next; ++p)
						*p = '.';
				}
			}
		}
#endif
		util_out_print("!AD", FALSE, p - buff, buff);
		if (is_string)
			util_out_print("\"", FALSE);
		if (bad_sub)
		{
			*ptr1 = ctemp;
			util_out_print("...", FALSE);
			break;
		}
		ptr = ++ptr1;
		if (*ptr)
			util_out_print("," ,FALSE);
		else
			break;
	}
	util_out_print(")", FALSE);
	return;
}
