/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "op.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "compiler.h"
#include "parm_pool.h"

GBLREF mv_stent			*mv_chain;
GBLREF unsigned char		*stackbase, *stacktop, *msp, *stackwarn;
GBLREF symval			*curr_symval;
GBLREF stack_frame		*frame_pointer;

error_def(ERR_ACTLSTTOOLONG);
error_def(ERR_STACKCRIT);
error_def(ERR_STACKOFLOW);

void op_bindparm(UNIX_ONLY_COMMA(int frmc) int frmp_arg, ...)
{
	va_list		var;
	uint4		mask;
	register lv_val *a;
	var_tabent	*parm_name;
	int		i;
	int		frmp;	/* formal argument pointer */
	VMS_ONLY(int	frmc;)	/* formal argument count */
	int		actc;
	unsigned int	*prev_count_ptr;
	unsigned int	prev_count;
	boolean_t	error = FALSE;
	lv_val		**actp;		/* actual pointer */
	lv_val		*new_var;
	mvs_ntab_struct	*ntab;
	ht_ent_mname	*tabent, **htepp;
	parm_slot	*curr_slot;
	DBGRFCT_ONLY(mident_fixed vname;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	frmp = frmp_arg;
	VMS_ONLY(va_count(frmc);)
	if (TREF(parm_pool_ptr))
	{
		curr_slot = PARM_CURR_SLOT;
		prev_count_ptr = &((*(curr_slot - 1)).mask_and_cnt.actualcnt);
		prev_count = *prev_count_ptr;
		/* If we were dealing with a new job, then push_parm would not have been called if the number of
		 * actuals was 0; so, by checking for proper frame value stored in the parameter pool we ensure
		 * that we are not looking at some uninitialized value here. Note that although we protect against
		 * fall-throughs into labels with a formallist now, in case two consecutive invocations to
		 * op_bindparm happen without a push_parm in between, do not attempt to use a previously utilized
		 * parameter set.
		 */
		if ((PARM_ACT_FRAME(curr_slot, prev_count) != frame_pointer) || (SAFE_TO_OVWRT <= prev_count))
			actc = 0;
		else
		{	/* Acquire mask, actual count, and pointer to actual list from the parameter pool. */
			mask = (*(curr_slot - 1)).mask_and_cnt.mask;
			actc = prev_count;
			if (0 == (TREF(parm_pool_ptr))->start_idx)
				actp = &((*(TREF(parm_pool_ptr))->parms).actuallist);
			else
				actp = &((*(curr_slot - SLOTS_NEEDED_FOR_SET(actc))).actuallist);
		}
	} else
		/* If the parameter pool is uninitialized, there are no parameters we can bind. */
		return;
	assert(0 <= frmc);
	/* This would also guarantee that actc > 0. */
	if (actc > frmc)
	{
		error = TRUE;
		*prev_count_ptr += SAFE_TO_OVWRT;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ACTLSTTOOLONG);
	}
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
	/* Incrementing the actual count in parameter pool by a special value, so that we know that it is safe to
	 * overwrite the current params and there is no need to save them; if an error occurred earlier or actc
	 * is 0, then this addition has already been taken care of above.
	 */
	if (actc && !error)
		*prev_count_ptr += SAFE_TO_OVWRT;
}
