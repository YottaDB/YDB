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

#ifndef __TP_UNWIND_H__
#define __TP_UNWIND_H__

enum tp_unwind_invocation
{
	COMMIT_INVOCATION = 1,
	ROLLBACK_INVOCATION,
	RESTART_INVOCATION
};

/* Array size to hold post unwind restored lv_vals to scan for containers differs between
   pro and dbg builds. For dbg, we want to exercise the code where more than one of the
   blocks is being processed but for production, we size the array where more than the
   one allocated in automatic storage would be "unusual".
*/
#ifdef DEBUG
#  define ARY_SCNCNTNR_DIM 1
#else
#  define ARY_SCNCNTNR_DIM 28
#endif
#define ARY_SCNCNTNR_MAX (ARY_SCNCNTNR_DIM - 1)

typedef struct post_restore_lvscan_struct
{
	struct post_restore_lvscan_struct *next;
	lv_val	*ary_scncntnr[ARY_SCNCNTNR_DIM];
	int	elemcnt;
} lvscan_blk;

void tp_unwind(uint4 newlevel, enum tp_unwind_invocation, int *tprestart_rc);
int tp_unwind_restlv(lv_val *curr_lv, lv_val *save_lv, tp_var *restore_ent, lvscan_blk **lvscan_anchor, int *tprestart_lc);

#endif
