/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "rtnhdr.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "error_trap.h"
#include "objlabel.h"
#include "cache.h"
#include "cache_cleanup.h"

#include "op.h"			/* for op_fntext() prototype */

GBLREF	stack_frame		*frame_pointer;
GBLREF	stack_frame		*error_frame;
GBLREF	unsigned char		*error_frame_mpc;
GBLREF	spdesc			stringpool;
GBLREF	unsigned char		*error_frame_save_mpc[DOLLAR_STACK_MAXINDEX];
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */

/* pos_str will contain "label+offset^routine" :: label & routine are midents, offset is an integer, "+" and "^" are literals */
#define MAX_POS_LEN	(2 * sizeof(mident) + MAX_DIGITS_IN_INT + STR_LIT_LEN("+^"))

void	get_frame_place_mcode(int level, int mode, int cur_zlevel, mval *result)
{
	int		count;
	stack_frame	*fp;
	unsigned char	pos_str[MAX_POS_LEN];
	mval		label;
	mval		routine;
	int		ips;
	int		offset;
	int		s1, s2;
	boolean_t	indirect_frame;
	ihdtyp		*irtnhdr;
	cache_entry	*indce;
	int4		*vp;
	unsigned char	*fpmpc;

	assert(DOLLAR_STACK_PLACE == mode || DOLLAR_STACK_MCODE == mode);
	assert(0 <= level);
	assert(level < cur_zlevel);
	count = cur_zlevel;
	for (fp = frame_pointer;  ; fp = fp->old_frame_pointer)
	{
		if (NULL == fp->old_frame_pointer)
		{
			assert(FALSE);
			result->str.len = 0;
			return;
		}
		if (!(fp->type & SFT_COUNT))
			continue;
		count--;
		if (count == level)
			break;
	}
	if (!(fp->flags & SFF_INDCE))
	{
		if ((dollar_ecode.error_rtn_addr != fp->mpc) || (DOLLAR_STACK_MAXINDEX <= level)
							|| (NULL == error_frame_save_mpc[level]))
			fpmpc = fp->mpc;
		else
			fpmpc = error_frame_save_mpc[level];
		result->str.addr = (char *)&pos_str[0];
		result->str.len = symb_line(fpmpc, &pos_str[0], 0, fp->rvector) - &pos_str[0];
		indirect_frame = FALSE;
	} else
	{
		indirect_frame = TRUE;
		pos_str[0] = '@';
		result->str.addr = (char *)&pos_str[0];
		result->str.len = 1;
	}
	assert((0 != result->str.len) || (DOLLAR_STACK_MAXINDEX <= level));
	if (DOLLAR_STACK_PLACE == mode)
	{
		s2pool(&result->str);
		assert(((unsigned char *)result->str.addr + result->str.len) == stringpool.free);
	}
	if (DOLLAR_STACK_MCODE == mode)
	{
		if (!indirect_frame)
		{
			label.mvtype = MV_STR;
			routine.mvtype = MV_STR;
			result->mvtype = MV_STR;
			label.str.len = result->str.len;
			label.str.addr = (char *)&pos_str[0];
			routine.str.len = 0;
			for (ips = 0, s1 = s2 = -1; ips < result->str.len; ips++)
			{
				if ('+' == pos_str[ips])
				{
					assert((-1 == s1) && (-1 == s2));
					s1 = ips;
				}
				if ('^' == pos_str[ips])
				{
					s2 = ips;
					break;
				}
			}
			if (s2 >= 0)
			{
				routine.str.addr = (char *)&pos_str[s2 + 1];
				routine.str.len = result->str.len - s2 - 1;
				label.str.len = s2;
			}
			offset = 0;
			if (s1 >= 0)
			{
				label.str.len = s1;
				if (s2 < 0)
					s2 = result->str.len;
				for (ips = s1 + 1; ips < s2; ips++)
					offset = offset * 10 + pos_str[ips] - '0';
			}
			op_fntext(&label, offset, &routine, result);
		} else
		{	/* code picked up from cache_cleanup(). any changes here might need to be reflected there */
			vp = (int4 *)fp->ctxt;
			vp--;
			assert((GTM_OMAGIC << 16) + OBJ_LABEL == *vp);	/* Validate backward linkage */
			vp--;
			irtnhdr = (ihdtyp *)((char *)vp + *vp);
			indce = irtnhdr->indce;
			assert(0 < indce->refcnt);	/* currently used in the M stack better have a non-zero refcnt */
			result->str = indce->src;
			s2pool(&result->str);
			assert(((unsigned char *)result->str.addr + result->str.len) == stringpool.free);
		}
	}
	return;
}
