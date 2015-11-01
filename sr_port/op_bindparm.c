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


#include "hashdef.h"
#include "lv_val.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "op.h"

#include <varargs.h>

GBLREF mv_stent			*mv_chain;
GBLREF unsigned char		*stackbase, *stacktop, *msp, *stackwarn;
GBLREF symval			*curr_symval;
GBLREF stack_frame		*frame_pointer;

void op_bindparm(va_alist)
va_dcl
{
	va_list		var;
	uint4		mask;
	register lv_val *a;
	mv_stent	*mv_ent;
	mident		*labname;
	int		i;
	int		frmc, frmp;	/* formal argument count, formal argument pointer */
	int		actc;
	lv_val		**actp;		/* actual pointer */
	lv_val		**lspp;
	lv_val		*new_var;
	mvs_ntab_struct	*ntab;
	ht_entry	*hte;
	bool		new;
	error_def	(ERR_STACKOFLOW);
	error_def	(ERR_STACKCRIT);
	error_def	(ERR_ACTLSTTOOLONG);

	for (mv_ent = mv_chain; MVST_PARM != mv_ent->mv_st_type; mv_ent = (mv_stent *)((char *)mv_ent + mv_ent->mv_st_next))
		assert(mv_ent < (mv_stent *)frame_pointer);
	VAR_START(var);
	frmc = va_arg(var, int4);
	actc = mv_ent->mv_st_cont.mvs_parm.mvs_parmlist->actualcnt;
	actp = mv_ent->mv_st_cont.mvs_parm.mvs_parmlist->actuallist;
	mask = mv_ent->mv_st_cont.mvs_parm.mvs_parmlist->mask;
	if (actc > frmc)
		rts_error(VARLSTCNT(1) ERR_ACTLSTTOOLONG);
	for (i = 0, frmp = va_arg(var, int4); i < frmc; i++, frmp = va_arg(var, int4), actp++)
	{
		if (i >= actc)
		{
			PUSH_MV_STENT(MVST_PVAL);
			new_var = mv_chain->mv_st_cont.mvs_pval.mvs_val = lv_getslot(curr_symval);
			new_var->v.mvtype = 0;
			new_var->tp_var = NULL;
			new_var->ptrs.val_ent.children = 0;
			new_var->ptrs.val_ent.parent.sym = curr_symval;
			ntab = &mv_chain->mv_st_cont.mvs_pval.mvs_ptab;
		} else if (!(mask & 1 << i))
		{
			ntab = &((mvs_pval_struct *)*actp)->mvs_ptab;
			new_var = ((mvs_pval_struct *)*actp)->mvs_val;
		} else
		{
			PUSH_MV_STENT(MVST_NTAB);
			ntab = &mv_chain->mv_st_cont.mvs_ntab;
			new_var = *actp;
		}
		lspp = (lv_val **)&frame_pointer->l_symtab[frmp];	/* address of l_symtab entry */
		labname = &(((vent *)frame_pointer->vartab_ptr)[frmp]);
		assert(0 <= frmp && frmp < frame_pointer->vartab_len);
		hte = ht_put(&curr_symval->h_symtab, (mname *)labname, &new);
		if (new)
			lv_newname(hte, curr_symval);
		ntab->nam_addr = labname;				/* address of mident */
		ntab->lst_addr = 0;
		if (frame_pointer->l_symtab > (mval **)frame_pointer)	/* if symtab is from older frame */
			ntab->lst_addr = lspp;				/* save address of l_symtab entry */
		ntab->save_value = (lv_val *)hte->ptr;			/* address of original lv_val */
		hte->ptr = (char *)new_var;
		*lspp = (lv_val *)new_var;
	}
	free(mv_ent->mv_st_cont.mvs_parm.mvs_parmlist);
	mv_ent->mv_st_cont.mvs_parm.mvs_parmlist = 0;
}
