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

#include <stdarg.h>

#include "lv_val.h"
#include "undx.h"

/* pkeys MUST be a va_list initialized in the caller via a va_start */
unsigned char	*undx (lv_val *start, va_list pkeys, int cnt, unsigned char *buff, unsigned short size)
{
	static lvname_info_ptr  lvn_info = NULL;
	int			cur_subscr;

	if (!lvn_info)
		lvn_info = (lvname_info_ptr) malloc(SIZEOF(struct lvname_info_struct));
	lvn_info->total_lv_subs = cnt + 1;
	lvn_info->start_lvp = start;
	for (cur_subscr = 0;  cur_subscr < cnt;  cur_subscr++)
	{
		lvn_info->lv_subs[cur_subscr] = va_arg(pkeys, mval *);
		MV_FORCE_STR(lvn_info->lv_subs[cur_subscr]);
	}
	return(format_key_mvals(buff, size, lvn_info));
}
