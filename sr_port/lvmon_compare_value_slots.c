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
#include "send_msg.h"
#include "localvarmonitor.h"
#include "error.h"		/* For "gtm_fork_n_core" */

GBLREF int4		gtm_trigger_depth;

error_def(ERR_LVMONBADVAL);

/* Routine that for each local variable being monitored compares the values saved in the two given slots. The assumption is
 * that these slots have been loaded across multiple calls to lvmon_pull_values() in those slots. Variables must be the same
 * type and the same value within that type. Note numeric values are not at this time verified so we are only looking for
 * string types and seeing they have not changed. Note this routine just finds the problems and sends messages to syslog but
 * does not force the program to stop.
 *
 * Parameters:
 *   lvmon_idx1 - First index to be compared - assumed to be the "expected" value.
 *   lvmon_idx2 - Second index to be compared.
 *
 * If the values saved in the given slots are not the same, an assertpro is generated.
 */
void	lvmon_compare_value_slots(int lvmon_idx1, int lvmon_idx2)
{
	int		len, cnt;
	lvmon_var	*lvmon_var_p;
	lvmon_value_ent	*lvmon_val_ent1_p;
	lvmon_value_ent	*lvmon_val_ent2_p;
	boolean_t	error_seen;
	char		*dummy_str;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TREF(lvmon_active));
	if ((0 != gtm_trigger_depth) || !TREF(lvmon_active))
		return;					/* Avoid check if no values yet or triggers are active */
	error_seen = FALSE;
	DBGLVMON((stderr, "lvmon_compare_value_slots: Checking slot %d vs %d\n", lvmon_idx1, lvmon_idx2));
	/* For each of the local variables, compare the string values in the slots looking for corruption */
	for (cnt = TREF(lvmon_vars_count), lvmon_var_p = TREF(lvmon_vars_anchor); 0 < cnt; cnt--, lvmon_var_p++)
	{	/* First, see if we have an existing lv_val for this var and if it is valid */
		lvmon_val_ent1_p = &lvmon_var_p->values[lvmon_idx1 - 1];
		lvmon_val_ent2_p = &lvmon_var_p->values[lvmon_idx2 - 1];
		if ((MV_STR & lvmon_val_ent1_p->varlvval.v.mvtype) && (MV_STR & lvmon_val_ent2_p->varlvval.v.mvtype))
		{	/* Both have string values (not checking string vs number sort of thing at this time */
			len = lvmon_val_ent1_p->varvalue.len;
			DBGLVMON((stderr, "** lvmon_compare_value_slots: Checking strings for var %.*s\n",
				  lvmon_var_p->lvmv.var_name.len, lvmon_var_p->lvmv.var_name.addr));
			if ((len != lvmon_val_ent2_p->varvalue.len)
			    || (0 != memcmp(lvmon_val_ent1_p->varvalue.addr, lvmon_val_ent2_p->varvalue.addr, len)))
			{	/* Something in the value changed and not for the better */
				error_seen = TRUE;
				DBGLVMON((stderr, "!! lvmon_compare_value_slots: Strings don't match (%d vs %d). Expected value: "
					  "%.*s   Actual value: %.*s\n", lvmon_idx1, lvmon_idx2,
					  lvmon_val_ent1_p->varvalue.len, lvmon_val_ent1_p->varvalue.addr,
					  lvmon_val_ent2_p->varvalue.len, lvmon_val_ent2_p->varvalue.addr));
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_LVMONBADVAL, 8,
					     lvmon_var_p->lvmv.var_name.len, lvmon_var_p->lvmv.var_name.addr,
					     lvmon_idx1, lvmon_idx2,
					     lvmon_val_ent1_p->varvalue.len, lvmon_val_ent1_p->varvalue.addr,
					     lvmon_val_ent2_p->varvalue.len, lvmon_val_ent2_p->varvalue.addr);
				gtm_fork_n_core();		/* Generate a core at the failure point */
			}
		} else
		{
			DBGLVMON((stderr, "** lvmon_compare_value_slots: Comparison avoided for var %.*s - one or both of the "
				  "values not a string - idx%d type: 0x%04lx", lvmon_var_p->lvmv.var_name.len,
				  lvmon_var_p->lvmv.var_name.addr, lvmon_idx1, lvmon_val_ent1_p->varlvval.v.mvtype));
			DBGLVMON((stderr, "   idx%d type: 0x%04lx\n",lvmon_idx2, lvmon_val_ent2_p->varlvval.v.mvtype));

		}
	}
}
