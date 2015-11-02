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

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "format_targ_key.h"
#include "gvsub2str.h"

/* return a pointer that points after the last char added */
unsigned char *format_targ_key(unsigned char *out_char_ptr, int4 max_size, gv_key *key, boolean_t dollarc)
{
	unsigned char			*gvkey_char_ptr, *out_top, *work_char_ptr, work_buff[MAX_ZWR_KEY_SZ], *work_top;
	boolean_t			is_string;
	DEBUG_ONLY(unsigned char	*gvkey_top_ptr;)

	assert(12 < max_size);
	out_top = out_char_ptr + max_size - 2;	/* - 2, as could add comma left-paren or TWO double quotes between checks */
	gvkey_char_ptr = key->base;
	DEBUG_ONLY(gvkey_top_ptr = gvkey_char_ptr + key->end;)
	/* Ensure input key is well-formed (i.e. double null terminated) */
	assert(KEY_DELIMITER == *(gvkey_top_ptr - 1));
	assert(KEY_DELIMITER == *gvkey_top_ptr);
	/* The following assert (in the for loop) assumes that a global name will be able to fit in completely into any key.
	 * But that is not true. For exmaple I can have a maxkeysize of 10 and try to set a global variable name of length 20.
	 * That will have issues below. Until C9J10-003204 is fixed to handle long global names and small maxkeysizes, we
	 * let the below code stay as it is (asserts only) to avoid overheads (of if checks for whether end is reached) in pro.
	 * When that is fixed, it is possible, we see the key terminate before even the global name is finished. In that case,
	 * we should return without '(' or ')' in the formatted buffer. The caller will know this is a case of too long global name.
	 */
	for (*out_char_ptr++ = '^'; (*out_char_ptr = *gvkey_char_ptr++); out_char_ptr++)
		assert(gvkey_char_ptr <= gvkey_top_ptr);
	assert(gvkey_char_ptr <= gvkey_top_ptr);
	if (0 == *gvkey_char_ptr)		/* no subscipts */
		return (out_char_ptr);
	*out_char_ptr++ = '(';
	for ( ; ; )
	{
		assert(gvkey_char_ptr <= gvkey_top_ptr);
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
				{
					assert(FALSE);
					return (NULL);
				}
				*out_char_ptr++ = *work_char_ptr++;
			}
			if (is_string)
				*out_char_ptr++ = '"';
		}
		if (out_char_ptr >= out_top)
		{
			assert(FALSE);
			return (NULL);
		}
		for ( ; *gvkey_char_ptr++; )
			assert(gvkey_char_ptr <= gvkey_top_ptr);
		assert(gvkey_char_ptr <= gvkey_top_ptr);
		if (*gvkey_char_ptr)
			*out_char_ptr++ = ',';
		else
			break;
	}
	*out_char_ptr++ = ')';
	assert(gvkey_char_ptr <= gvkey_top_ptr);
	return (out_char_ptr);
}
