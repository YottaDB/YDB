/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "hashtab_mname.h"
#include "lv_val.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"
#include "op.h"
#include <stdarg.h>

GBLREF mv_stent			*mv_chain;
GBLREF unsigned char		*stackbase, *stacktop, *msp, *stackwarn;
GBLREF symval			*curr_symval;
GBLREF stack_frame		*frame_pointer;

void op_bindparm(UNIX_ONLY_COMMA(int frmc) int frmp_arg, ...)
{
	va_list		var;
	uint4		mask;
	register lv_val *a;
	mv_stent	*mv_ent;
	var_tabent	*parm_name;
	int		i;
	int		frmp;	/* formal argument pointer */
	VMS_ONLY(int	frmc;)	/* formal argument count */
	int		actc;
	lv_val		**actp;		/* actual pointer */
	lv_val		**lspp;
	lv_val		*new_var;
	mvs_ntab_struct	*ntab;
	ht_ent_mname	*tabent;
	error_def	(ERR_STACKOFLOW);
	error_def	(ERR_STACKCRIT);
	error_def	(ERR_ACTLSTTOOLONG);

	for (mv_ent = mv_chain; MVST_PARM != mv_ent->mv_st_type; mv_ent = (mv_stent *)((char *)mv_ent + mv_ent->mv_st_next))
		assert(mv_ent < (mv_stent *)frame_pointer);
	frmp = frmp_arg;
	actc = mv_ent->mv_st_cont.mvs_parm.mvs_parmlist->actualcnt;
	actp = mv_ent->mv_st_cont.mvs_parm.mvs_parmlist->actuallist;
	mask = mv_ent->mv_st_cont.mvs_parm.mvs_parmlist->mask;
	VMS_ONLY(va_count(frmc);)
	if (actc > frmc)
		rts_error(VARLSTCNT(1) ERR_ACTLSTTOOLONG);
	VAR_START(var, frmp_arg);
	for (i = 0; i < frmc; i++, frmp = va_arg(var, int4), actp++)
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
		parm_name = &(((var_tabent *)frame_pointer->vartab_ptr)[frmp]);
		assert(0 <= frmp && frmp < frame_pointer->vartab_len);
		if (add_hashtab_mname(&curr_symval->h_symtab, parm_name, NULL, &tabent))
			lv_newname(tabent, curr_symval);
		ntab->nam_addr = parm_name;				/* address of var_tabent */
		ntab->lst_addr = 0;
		if (frame_pointer->l_symtab > (mval **)frame_pointer)	/* if symtab is from older frame */
			ntab->lst_addr = lspp;				/* save address of l_symtab entry */
		ntab->save_value = (lv_val *)tabent->value;			/* address of original lv_val */
		tabent->value = (char *)new_var;
		*lspp = (lv_val *)new_var;
	}
	va_end(var);
	free(mv_ent->mv_st_cont.mvs_parm.mvs_parmlist);
	mv_ent->mv_st_cont.mvs_parm.mvs_parmlist = 0;
}
