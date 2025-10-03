/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gtmio.h"
#include "lv_val.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "tp_frame.h"
#include "op.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*stackbase, *stacktop, *msp, *stackwarn;
GBLREF stack_frame	*frame_pointer;
GBLREF tp_frame		*tp_pointer;
GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

/* Note this module follows the same basic pattern as gtm_newintrisic which handles
 * the same function but for intrinsic vars instead of local vars.
 */
void op_newvar(uint4 arg1)
{
	mv_stent 	*mv_st_ent, *mvst_tmp, *mvst_prev;
	ht_ent_mname	*tabent;
	stack_frame	*fp, *fp_prev, *fp_fix;
	unsigned char	*old_sp, *top;
	lv_val		*new;
	var_tabent	*varname, lcl_varname;
	mvs_ntab_struct *ptab;
	tp_frame	*tpp;
	int		indx;
	int4		shift_size;
	DBGRFCT_ONLY(mident_fixed vname;)

	varname = &(((var_tabent *)frame_pointer->vartab_ptr)[arg1]);
#	ifdef DEBUG
	if ((INDIR_MARKED != varname->marked) && DYNAMIC_VARNAMES_ACTIVE(frame_pointer))
	{
		lcl_varname = *varname;
		RELOCATE(lcl_varname.var_name.addr, char *, frame_pointer->rvector->literal_text_adr);
		tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &lcl_varname);
		/* Don't reassign varname, so any dangling reference to it created below needs fixup if DYNAMIC_VARNAMES */
	} else
		tabent = lookup_hashtab_mname(&curr_symval->h_symtab, varname);
#	endif
	assert(tabent);	/* variable must be defined and fetched by this point */
	assert(tabent == frame_pointer->l_symtab[arg1]);
	tabent = frame_pointer->l_symtab[arg1];
	/*
	 * If the var being new'd exists in an earlier frame, we need to save
	 * that value so it can be restored when we exit this frame. Create a
	 * stack entry to save the old value. If there was no previous entry,
	 * we will destroy the entry when we pop off this frame (make it
	 * undefined again).
	 */
	if (!(frame_pointer->flags & SFF_INDCE))
	{	/* This is a normal counted frame with a stable variable name pointer */
		PUSH_MV_STENT(MVST_PVAL);
		mv_st_ent = mv_chain;
		new = mv_st_ent->mv_st_cont.mvs_pval.mvs_val = lv_getslot(curr_symval);
		ptab = &mv_st_ent->mv_st_cont.mvs_pval.mvs_ptab;
	} else
	{	/* This is actually an indirect (likely XECUTE or ZINTERRUPT) so the varname
		 * pointer could be gone by the time we unroll this frame if an error occurs
		 * while this frame is executing and error processing marks this frame reusable
		 * so carry the name along with us to avoid this situation.
		 */
		PUSH_MV_STENT(MVST_NVAL);
		mv_st_ent = mv_chain;
		new = mv_st_ent->mv_st_cont.mvs_nval.mvs_val = lv_getslot(curr_symval);
		ptab = &mv_st_ent->mv_st_cont.mvs_nval.mvs_ptab;
		DEBUG_ONLY(mv_st_ent->mv_st_cont.mvs_nval.name = tabent->key;)
		DEBUG_ONLY(varname = &mv_st_ent->mv_st_cont.mvs_nval.name);
	}
	assert((int)arg1 >= 0);
	/* Initialize new data cell */
	LVVAL_INIT(new, curr_symval);
	/* Finish initializing restoration structures */
	ptab->save_value = (lv_val *)tabent->value;
	ptab->hte_addr = tabent;
#ifdef	DEBUG
	ptab->nam_addr = varname;
	/* Strictly speaking the following is only necessary in the PVAL case, when a flush_jmp retains the PVAL but clobbers
	 * the rvector and makes the relative offset no longer accurate. So store the literal_text_adr for later use.
	 */
	ptab->cur_ltext_addr = DYNAMIC_VARNAMES_ACTIVE(frame_pointer) ? frame_pointer->rvector->literal_text_adr : NULL;
#endif
	DBGRFCT_ONLY(
		memcpy(vname.c, tabent->key.var_name.addr, tabent->key.var_name.len);
		vname.c[tabent->key.var_name.len] = '\0';
	);
	DBGRFCT((stderr, "op_newvar: Var '%s' hte 0x"lvaddr" being reset from 0x"lvaddr" to 0x"lvaddr"\n",
		 &vname.c, tabent, tabent->value, new));
	tabent->value = (char *)new;
}
