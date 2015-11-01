/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "mlkdef.h"
#include "hashdef.h"
#include "lv_val.h"
#include "op.h"
#include "tp_unwind.h"
#include "mlk_pvtblk_delete.h"

error_def(ERR_STACKUNDERFLO);

GBLREF	stack_frame	*frame_pointer;
GBLREF	unsigned char	*msp, *stackbase, *stacktop;
GBLREF	mv_stent	*mv_chain;

GBLREF	tp_frame	*tp_pointer;
GBLREF	unsigned char	*tp_sp, *tpstackbase, *tpstacktop;
GBLREF	mlk_pvtblk	*mlk_pvt_root;
GBLREF	short		dollar_tlevel;



void	tp_unwind(short newlevel, bool restore)
{
	mlk_pvtblk	**prior, *mlkp;
	mlk_tp		*oldlock, *nextlock;
	int		tl;
	lv_val		*save_lv, *curr_lv;
	tp_var		*restore_ent;
	mv_stent	*mvc;

	assert(tp_sp <= tpstackbase  &&  tp_sp > tpstacktop);
	assert(tp_pointer <= (tp_frame *)tpstackbase  &&  tp_pointer > (tp_frame *)tpstacktop);
	for (tl = dollar_tlevel;  tl > newlevel;  --tl)
	{
		for (restore_ent = tp_pointer->vars;  NULL != restore_ent;  restore_ent = tp_pointer->vars)
		{
			/******************************************************************************/
			/* TP_VAR_CLONE unsets the tp_var flag, showing that the tree has been cloned */
			/* If tp_var is still set, it shows that curr_lv and save_lv are still sharing*/
			/* the tree, so it should not be killed                                       */
			/******************************************************************************/

			curr_lv = restore_ent->current_value;
			save_lv = restore_ent->save_value;

			assert(NULL != save_lv);
			assert(MV_SYM == save_lv->ptrs.val_ent.parent.sym->ident);

			/* In order to restart sub-transactions, this would have to maintain
			   the chain that currently is not built by op_tstart() */
			if (restore)
			{
				if ((NULL == curr_lv->tp_var) && (NULL != curr_lv->ptrs.val_ent.children))
					lv_killarray(curr_lv->ptrs.val_ent.children);
				*curr_lv = *save_lv;
				if (curr_lv->ptrs.val_ent.children != NULL)
					curr_lv->ptrs.val_ent.children->lv = curr_lv;
			} else if ((NULL == curr_lv->tp_var) && (NULL != save_lv->ptrs.val_ent.children))
			{
				save_lv->ptrs.val_ent.children->lv = save_lv;
				save_lv->tp_var = NULL;
				op_kill(save_lv);
			}
			curr_lv->tp_var = NULL;
			save_lv->v.mvtype = 0;
			save_lv->ptrs.val_ent.children = NULL;
			save_lv->tp_var = NULL;
			save_lv->ptrs.free_ent.next_free = save_lv->ptrs.val_ent.parent.sym->lv_flist;
			save_lv->ptrs.val_ent.parent.sym->lv_flist = save_lv;

			tp_pointer->vars = restore_ent->next;
			free(restore_ent);
		}
		if (tp_pointer->fp == frame_pointer && mv_chain->mv_st_type == MVST_TPHOLD && msp == (unsigned char *)mv_chain)
			POP_MV_STENT();
		if (NULL == tp_pointer->old_tp_frame)
			tp_sp = tpstackbase;
		else
			tp_sp = (unsigned char *)tp_pointer->old_tp_frame;
		if (tp_sp > tpstackbase)
			rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
		if (NULL != tp_pointer->sym)
			--tp_pointer->sym->tp_save_all;
		if ((NULL != (tp_pointer = tp_pointer->old_tp_frame))
			&& ((tp_pointer < (tp_frame *)tp_sp) || (tp_pointer > (tp_frame *)tpstackbase)
			|| (tp_pointer < (tp_frame *)tpstacktop)))
				rts_error(VARLSTCNT(1) ERR_STACKUNDERFLO);
	}
	if ((0 != newlevel) && restore)
	{	/* Restore current context */
		for (restore_ent = tp_pointer->vars;  NULL != restore_ent;  restore_ent = restore_ent->next)
		{
			curr_lv = restore_ent->current_value;
			save_lv = restore_ent->save_value;
			assert(NULL != save_lv);
			if ((NULL == curr_lv->tp_var) && (NULL != curr_lv->ptrs.val_ent.children))
				lv_killarray(curr_lv->ptrs.val_ent.children);
			*curr_lv = *save_lv;
			if (NULL != curr_lv->ptrs.val_ent.children)
				curr_lv->ptrs.val_ent.children->lv = curr_lv;
		}
	}
	for (prior = &mlk_pvt_root, mlkp = *prior;  NULL != mlkp;  mlkp = *prior)
	{
		if (mlkp->granted)
		{	/* This was a pre-existing lock */
			for (oldlock = mlkp->tp;  (NULL != oldlock) && ((int)oldlock->tplevel >= newlevel);  oldlock = nextlock)
			{
				nextlock = oldlock->next;
				free(oldlock);
			}
			mlkp->tp = oldlock;
			prior = &mlkp->next;
		} else
			mlk_pvtblk_delete(prior);
	}
	dollar_tlevel = newlevel;
}
