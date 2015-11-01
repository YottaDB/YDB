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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "format_targ_key.h"
#include "gvsub2str.h"

/* return a pointer that points after the last char added */
unsigned char *format_targ_key(unsigned char *out_char_ptr, int4 max_size, gv_key *key, bool dollarc)
{
	unsigned char	*gvkey_char_ptr, *out_top, *work_char_ptr, work_buff[MAX_ZWR_KEY_SZ], *work_top;
	boolean_t	is_string;

	assert(max_size > 12);
	out_top = out_char_ptr + max_size - 2;	/* - 2, as could add comma left-paren or TWO double quotes between checks */
	gvkey_char_ptr = key->base;
	*out_char_ptr++ = '^';
	for (;  (*out_char_ptr = *gvkey_char_ptr++);  out_char_ptr++)
		;
	if (!*gvkey_char_ptr)		/* no subscipts */
		return (out_char_ptr);
	*out_char_ptr++ = '(';
	for(;;)
	{
		if (0x01 == *gvkey_char_ptr)	/* this must be a null string which was adjusted by op_gvorder */
		{
			*out_char_ptr++ = '"';
			*out_char_ptr++ = '"';
		} else
		{
			is_string = FALSE;
			if ((STR_SUB_PREFIX == *gvkey_char_ptr) && !dollarc)
			{
				is_string = TRUE;
				*out_char_ptr++ = '"';
			}
			work_top = gvsub2str(gvkey_char_ptr, work_buff, dollarc);
			for (work_char_ptr = work_buff;  work_char_ptr < work_top;)
			{
				if (out_char_ptr >= out_top)
					return (NULL);
				*out_char_ptr++ = *work_char_ptr++;
			}
			if (is_string)
				*out_char_ptr++ = '"';
		}
		if (out_char_ptr >= out_top)
			return (NULL);
		for(;  *gvkey_char_ptr++;)
			;
		if (*gvkey_char_ptr)
			*out_char_ptr++ = ',';
		else
			break;
	}
	*out_char_ptr++ = ')';
	return (out_char_ptr);
}
