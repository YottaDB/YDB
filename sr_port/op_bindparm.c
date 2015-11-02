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
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "lv_val.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "stack_frame.h"
#include "op.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

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
	lv_val		*new_var;
	mvs_ntab_struct	*ntab;
	ht_ent_mname	*tabent, **htepp;
	DBGRFCT_ONLY(mident_fixed vname;)

	error_def(ERR_STACKOFLOW);
	error_def(ERR_STACKCRIT);
	error_def(ERR_ACTLSTTOOLONG);

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
		{	/* "Extra" formallist parm, not part of actual list - Needs a PVAL (implicit NEW) as doesn't have one yet */
			PUSH_MV_STENT(MVST_PVAL);
			new_var = mv_chain->mv_st_cont.mvs_pval.mvs_val = lv_getslot(curr_symval);
			LVVAL_INIT(new_var, curr_symval);
			ntab = &mv_chain->mv_st_cont.mvs_pval.mvs_ptab;
			ntab->hte_addr = NULL;		/* In case table gets expanded before we set it below */
		} else if (!(mask & 1 << i))
		{	/* Actual list parm - already has PVAL built by push_parm() */
			ntab = &((mvs_pval_struct *)*actp)->mvs_ptab;
			new_var = ((mvs_pval_struct *)*actp)->mvs_val;
		} else
		{	/* Actual list parm - dotted pass-by-reference parm */
			PUSH_MV_STENT(MVST_NTAB);
			ntab = &mv_chain->mv_st_cont.mvs_ntab;
			new_var = *actp;
			/* This sort of parameter is considered an alias */
			assert(0 < new_var->stats.trefcnt);
			INCR_TREFCNT(new_var);
		}
		htepp = (ht_ent_mname **)&frame_pointer->l_symtab[frmp];	/* address of l_symtab entry */
		parm_name = &(((var_tabent *)frame_pointer->vartab_ptr)[frmp]);
		assert(0 <= frmp && frmp < frame_pointer->vartab_len);
		if (add_hashtab_mname_symval(&curr_symval->h_symtab, parm_name, NULL, &tabent))
			lv_newname(tabent, curr_symval);
		assert(tabent->value);
		DEBUG_ONLY(ntab->nam_addr = parm_name);				/* address of var_tabent */
		ntab->hte_addr = tabent;					/* save hash table entry addr */
		ntab->save_value = (lv_val *)tabent->value;			/* address of original lv_val */
		DBGRFCT_ONLY(
			memcpy(vname.c, tabent->key.var_name.addr, tabent->key.var_name.len);
			vname.c[tabent->key.var_name.len] = '\0';
		);
		DBGRFCT((stderr, "op_bindparm: Resetting var '%s' with hte 0x"lvaddr" from 0x"lvaddr" to 0x"lvaddr"\n",
			 &vname.c, tabent, tabent->value, new_var));
		tabent->value = (char *)new_var;
		*htepp = tabent;
	}
	va_end(var);
	free(mv_ent->mv_st_cont.mvs_parm.mvs_parmlist);
	mv_ent->mv_st_cont.mvs_parm.mvs_parmlist = NULL;
}
