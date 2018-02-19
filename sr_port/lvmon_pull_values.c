/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information 		*
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
#include "io.h"
#include "lv_val.h"
#include "localvarmonitor.h"

#define LVMON_WBOX_BREAK_VALUE "This is a white box test string"
#define LVMON_SAVE_VALUE(TYPE, VALUELEN, VALUE)								\
{													\
	DBGLVMON((stderr, "** lvmon_pull_values: Fetch "TYPE" string value for variable %.*s\n",	\
		  lvmon_var_p->lvmv.var_name.len, lvmon_var_p->lvmv.var_name.addr));			\
	lvmon_val_ent_p->varvalue.len = len = (VALUELEN);						\
	if (0 < len)											\
	{	/* Need to copy the string to malloc'd space. If current buffer can hold it,		\
		 * use it - else release (if exists) and malloc a new one.				\
		 */											\
		if (len > lvmon_val_ent_p->alloclen)							\
		{	/* Length of buffer is too small, release and realloc */			\
			DBGLVMON((stderr, "** lvmon_pull_values: Reallocating buffer\n"));		\
			if (lvmon_val_ent_p->alloclen)							\
				free(lvmon_val_ent_p->varvalue.addr);					\
			lvmon_val_ent_p->varvalue.addr = malloc(len);					\
			lvmon_val_ent_p->alloclen = len;						\
		}											\
		assert(len >= lvmon_val_ent_p->varvalue.len);						\
		memcpy(lvmon_val_ent_p->varvalue.addr, (VALUE), len);					\
	}												\
}

GBLREF int4	gtm_trigger_depth;
GBLREF symval	*curr_symval;

/* Routine to pull the current values of a defined list of local vars and store it in one of the value slots.
 *
 * Parameters:
 *   lvmon_p	     - Pointer to lvmon array for which we are grabbing values.
 *   lvmon_value_idx - Which value index in the lvmon.varvalues[] array to use to store the values.
 *
 * Return value meanings:
 *   0 - All values fetched
 */
void lvmon_pull_values(int lvmon_ary_idx)
{
	int		cnt, size, len;
	lvmon_var	*lvmon_var_p;
	ht_ent_mname	*tabent;
	lv_val		*lv_val_p;
	lvmon_value_ent	*lvmon_val_ent_p;
	DEBUG_ONLY(boolean_t lvmon_wbtest_break_mstr;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(lvmon_active));
	if ((0 != gtm_trigger_depth) || !TREF(lvmon_active))
		return;				/* Don't record trigger values as they disappear when trigger done */
	DEBUG_ONLY(lvmon_wbtest_break_mstr = FALSE);
	DBGLVMON((stderr,"lvmon_pull_values: Pulling values into slot %d\n",lvmon_ary_idx));
	/* Fetch the value of each of the local variables and store them in the values array in the specified index */
	for (cnt = TREF(lvmon_vars_count), lvmon_var_p = TREF(lvmon_vars_anchor); 0 < cnt; cnt--, lvmon_var_p++)
	{	/* First, see if we have an existing lv_val for this var and if it is valid */
		lvmon_val_ent_p = &lvmon_var_p->values[lvmon_ary_idx - 1];
		if ((NULL == lvmon_var_p->varlvadr) || (TREF(curr_symval_cycle) != lvmon_var_p->curr_symval_cycle))
		{	/* Either lv_val pointer does not exist or is no longer valid so needs to be re-located */
			DBGLVMON((stderr, "** lvmon_pull_values: (Re)Pull lv_val addr for %.*s\n",
				  lvmon_var_p->lvmv.var_name.len, lvmon_var_p->lvmv.var_name.addr));
			tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &lvmon_var_p->lvmv);
			if (NULL == tabent)
			{	/* Variable does not exist in this symbol table so clear the array index for the value
				 * and continue on. Note do not clear the alloclen field or the address of the value in
				 * varvalue to avoid a memory leak.
				 */
				DBGLVMON((stderr, "** lvmon_pull_values: lv_val for %.*s not found (yet)\n",
					  lvmon_var_p->lvmv.var_name.len, lvmon_var_p->lvmv.var_name.addr));
				memset(&lvmon_val_ent_p->varlvval, 0, SIZEOF(lv_val));
				lvmon_val_ent_p->varvalue.len = 0;
				continue;
			}
			/* Update the fields */
			lvmon_var_p->curr_symval_cycle = TREF(curr_symval_cycle);
			lvmon_var_p->varlvadr = lv_val_p = ((lv_val *)tabent->value);
		} else
			lv_val_p = lvmon_var_p->varlvadr;
		lvmon_val_ent_p->varlvval = *lv_val_p;		/* Save entire previous lv_val */
		/* Common code if lv_val is string - update fields in the specified index of the value array */
		if (MV_IS_STRING(&lv_val_p->v))
		{	/* We have a string, see about storing it in a malloc'd buffer. Since we are going
			 * to be monitoring stringpool garbage collection, it is best to keep our comparison
			 * values out of the stringpool so we can avoid having "stp_gcol" process our value
			 * array. Note every $gtm_white_box_test_case_count times, we will copy he wrong value
			 * to force an error to occur.
			 */
#			ifdef DEBUG
			GTM_WHITE_BOX_TEST(WBTEST_LVMON_PSEUDO_FAIL, lvmon_wbtest_break_mstr, TRUE);
			if (lvmon_wbtest_break_mstr)
			{
				LVMON_SAVE_VALUE("**FAKE**", SIZEOF(LVMON_WBOX_BREAK_VALUE) - 1,
						 LVMON_WBOX_BREAK_VALUE);
			} else
#			endif
			{
				LVMON_SAVE_VALUE("current string", lv_val_p->v.str.len, lv_val_p->v.str.addr);
			}
			DEBUG_ONLY(lvmon_wbtest_break_mstr = FALSE);
		} else
			DBGLVMON((stderr, "** lvmon_pull_values: Value not a string: 0x%04lx\n", lv_val_p->v.mvtype));
	}
	return;
}
