/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"

#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gtmctype.h"
#include "cli.h"
#include "dse.h"
#include "gvsub2str.h"
#include "zshow.h"

GBLREF enum dse_fmt	dse_dmp_format;
GBLREF gd_region	*gv_cur_region;
GBLREF char	 	patch_comp_key[MAX_KEY_SZ + 1];

static unsigned char	*work_buff;
static unsigned int	work_buff_length;

boolean_t dse_fdmp(sm_uc_ptr_t data, int len)
{
	unsigned char	*key_char_ptr, *work_char_ptr;
	int 		dest_len;

	if (work_buff_length < ZWR_EXP_RATIO(gv_cur_region->max_rec_size))
	{
		work_buff_length = ZWR_EXP_RATIO(gv_cur_region->max_rec_size);
		if (work_buff)
			free (work_buff);
		work_buff = (unsigned char *)malloc(work_buff_length);
	}
	work_char_ptr = work_buff;
	*work_char_ptr++ = '^';
	for (key_char_ptr = (uchar_ptr_t)patch_comp_key; *key_char_ptr ; key_char_ptr++)
	{
		if (PRINTABLE(*key_char_ptr))
			*work_char_ptr++ = *key_char_ptr;
		else
			return FALSE;
	}
	key_char_ptr++;
	if (*key_char_ptr)
	{
		*work_char_ptr++ = '(';
		for (;;)
		{
			work_char_ptr = gvsub2str(key_char_ptr, work_char_ptr, TRUE);
			/* Removed unnecessary checks for printable characters (PRINTABLE()) here
			 * since the data being written into files (OPENed files) would have been
			 * passed through ZWR translation which would have taken care of converting
			 * to $CHAR() or $ZCHAR() */

			for (; *key_char_ptr ; key_char_ptr++)
				;
			key_char_ptr++;
			if (*key_char_ptr)
				*work_char_ptr++ = ',';
			else
				break;
		}
		*work_char_ptr++ = ')';
	}
	assert(MAX_ZWR_KEY_SZ >= work_char_ptr - work_buff);
	if (GLO_FMT == dse_dmp_format)
	{
		if (!dse_fdmp_output(work_buff, (int4)(work_char_ptr - work_buff)))
			return FALSE;
		if (!dse_fdmp_output(data, len))
			return FALSE;
	} else
	{
		assert(ZWR_FMT == dse_dmp_format);
		*work_char_ptr++ = '=';
		format2zwr(data, len, work_char_ptr, &dest_len);
		if (!dse_fdmp_output(work_buff, (int4)(work_char_ptr + dest_len - work_buff)))
			return FALSE;
	}
	return TRUE;
}
