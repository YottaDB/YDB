/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *--------------------------------------------------------------------------
 * Descriptipn:
 * 	Given the lvname_info *, which contains all the mvals of all
 * 	subscripts and the name itself, of a local variable node,
 *	this will format the entire local variable key
 * Input:
 *	lvnp: Pointer to the structure of a local variable
 * 	buff: Buffer where key will be formatted
 * 	size: Size of buff
 * Return Value:
 *	End address upto which buffer was  used to format the key
 *	(Needed for the length calculation in caller)
 *--------------------------------------------------------------------------
 */

#include "mdef.h"

#include "gtm_string.h"
#include "lv_val.h"		/* needed by "lv_nameinfo.h" */
#include "op.h"			/* for "op_fnzwrite" prototype */

unsigned char	*format_key_mvals(unsigned char *buff, int size, lvname_info *lvnp)
{
	int		cnt;
	mval		*keys, dst;
	int		n, subcnt;
	unsigned char	*endbuff;

	cnt = (int)lvnp->total_lv_subs - 1;
	endbuff = format_lvname(lvnp->start_lvp, buff, size);
	size -= (int)(endbuff - buff);
	buff = endbuff;

	if (cnt > 0 && size > 0)
	{
		*buff++ = '(';
		size--;
		subcnt = 0;
		for (n = 0;  ;  )
		{
			keys = lvnp->lv_subs[subcnt++];
			op_fnzwrite(0, keys, &dst);	/* Store ZWRITE representation of keys in "dst" */
			/* dst now points to stringpool */
			MV_FORCE_STR(keys);
			if (size > dst.str.len)
			{
				assert(MVTYPE_IS_STRING(dst.mvtype));
				memcpy(buff, dst.str.addr, dst.str.len);
				buff += dst.str.len;
				size -= dst.str.len;
			} else
			{
				/* copy as much space as we have */
				memcpy(buff, dst.str.addr, size);
				buff += size;
				break;
			}
			if (++n < cnt  &&  size > 0)
			{
				*buff++ = ',';
				size--;
			} else
			{
				if (size > 0)
				{
					*buff++ = ')';
					size--;
				}
				break;
			}
		}
	}
	return buff;
}
