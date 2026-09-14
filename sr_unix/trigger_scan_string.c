/****************************************************************
 *								*
 * Copyright (c) 2010-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "trigger_scan_string.h"

boolean_t trigger_scan_string(char *src_ptr, uint4 *src_len, char *dst_ptr, uint4 *dst_len)
{
	uint4 d_len, max_len, s_len;

	s_len = *src_len;
	max_len = *dst_len;	/* cff3d614-03d3-4734-ba12-291f8ebbf5c8 */
	*dst_len = 0;
	if (1 >= s_len)
	{ /* Invalid string - it needs at least "" */
		return FALSE;
	}
	if ('"' != *src_ptr)
	{ /* String needs to start with " */
		return FALSE;
	}
	++src_ptr;
	s_len--;
	d_len = 0;
	while ((0 < s_len) && (max_len > d_len))
	{ /* Scan until the closing quote */
		if ('"' == *src_ptr)
		{
			if (1 == s_len)
				break;
			if ('"' == *(src_ptr + 1))
			{
				src_ptr++;
				s_len--;
			} else
				break;
		}
		*dst_ptr++ = *src_ptr++;
		d_len++;
		s_len--;
	}
	*dst_len = d_len;
	*src_len = s_len;
	return ('"' == *src_ptr);
}
