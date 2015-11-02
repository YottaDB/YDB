/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef GTM_TRIGGER

#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"
#include "gvcst_protos.h"
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "trigger.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "targ_alloc.h"			/* for SETUP_TRIGGER_GLOBAL & SWITCH_TO_DEFAULT_REGION */
#include "filestruct.h"			/* for INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED (FILE_INFO) */
#include "mvalconv.h"
#include "gdscc.h"			/* needed for tp.h */
#include "gdskill.h"			/* needed for tp.h */
#include "buddy_list.h"			/* needed for tp.h */
#include "hashtab_int4.h"		/* needed for tp.h */
#include "jnl.h"			/* needed for tp.h */
#include "tp.h"				/* for sgm_info */
#include "tp_frame.h"
#include "tp_restart.h"
#include "tp_set_sgm.h"
#include "t_retry.h"
#include "op.h"
#include "op_tcommit.h"
#include "memcoherency.h"
#include "gtmimagename.h"
#include "trigger_fill_xecute_buffer.h"
#include "trigger_gbl_fill_xecute_buffer.h"

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_currkey;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	gv_namehead		*gv_target;
GBLREF	boolean_t		skip_INVOKE_RESTART;
GBLREF	int			tprestart_state;
GBLREF	int4			tstart_trigger_depth;
GBLREF	int4			gtm_trigger_depth;
GBLREF	tp_frame		*tp_pointer;

LITREF	mval			literal_hasht;

error_def(ERR_TRIGNAMBAD);
error_def(ERR_TPRETRY);

STATICFNDCL CONDITION_HANDLER(trigger_fill_xecute_buffer_ch);
STATICFNDCL void trigger_fill_xecute_buffer_read_trigger_source(gv_trigger_t *trigdsc);

/* Similar condition handler to above without the tp-restart - just unwind and let caller do the restart */
CONDITION_HANDLER(trigger_fill_xecute_buffer_ch)
{
	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		UNWIND(NULL, NULL);
	}
	NEXTCH;
}

int trigger_fill_xecute_buffer(gv_trigger_t *trigdsc)
{
	int	src_fetch_status;

	assert(!dollar_tlevel || (tstart_trigger_depth <= gtm_trigger_depth));
	/* We have 3 cases to consider - all of which REQUIRE a TP fence to already be in effect. The reason for this is, if we
	 * detect a restartable condition, we are going to cause this region's triggers to be unloaded which destroys the block
	 * our parameter is pointing to so the restart logic MUST take place outside of this routine.
	 *
	 *   1. We have an active transaction due to an IMPLICIT TSTART done by trigger handling but the trigger level has not yet
	 *	been created. We don't need another TP wrapper in this case but we do need a condition handler to trap the thrown
	 *	retry to again prevent C stack unwind and return to the caller in the same shape that gtm_trigger would return.
	 *   2. We have an active transaction due to an EXPLICIT M-code TSTART command. For this case, the trigger loads proceed
	 *   	as normal with restarts handled in the regular automatic fashion. Note this case also covers the tp restarts done
	 *	by both the update process and mupip recover forward since those functions have their own way of intercepting and
	 *	dealing with restarts. To cover those cases, tp->implicit_tstart can be TRUE but tp_implicit_trigger MUST be
	 *	FALSE.
	 *   3. We have an active transaction due to an IMPLICIT TSTART done by trigger handling and one or more triggers are
	 *	running. This becomes like case 2 since the restart will be handled by gtm_trigger and the proper thing will
	 *	be done.
	 *
	 * An extra note about case 3. Case 3 can be the identified case if in a nested trigger we are in trigger-no-mans-land
	 * with a base frame for the nested trigger (having driven one of a set of parallel nested triggers) but no actual trigger
	 * execution frame yet exists. This is really a case 1 situation with a nested trigger but it turns out that dealing with
	 * like case 3 does the right thing because if/when mdb_condition_handler catches a thrown TPRETRY error, mdb_condition
	 * handler will peal the nested trigger frame off before doing the restart which works for us and avoids issues of
	 * multi-level implicit restarts we would otherwise have to handle.
	 *
	 * Note, this routine is for loading trigger source when the trigger is to be driven. The trigger_source_read_andor_verify()
	 * routine should be used when fetching trigger source for reasons other than driving the triggers. This routine is lighter
	 * weight but has a dependence on the restartability of the trigger-drive logic for getting the triggers reloaded as
	 * necessary.
	 */
	if (0 < dollar_tlevel)
	{
		if (!tp_pointer->implicit_trigger		/* Case 2 */
		    || (tp_pointer->implicit_tstart && tp_pointer->implicit_trigger
			&& (tstart_trigger_depth != gtm_trigger_depth)))	/* Case 3 */
		{	/* Test for Case 3/4 where we get to do very little: */
			assert((!tp_pointer->implicit_trigger) || (0 < gtm_trigger_depth));
			trigger_fill_xecute_buffer_read_trigger_source(trigdsc);
		} else
		{	/* Test for Case 1 where we only need a condition handler */
			assert(tp_pointer->implicit_tstart && tp_pointer->implicit_trigger);
			assert(tstart_trigger_depth == gtm_trigger_depth);
			ESTABLISH_RET(trigger_fill_xecute_buffer_ch, SIGNAL);
			trigger_fill_xecute_buffer_read_trigger_source(trigdsc);
			REVERT;
		}
	} else
		GTMASSERT;
	/* return our bounty to caller */
	trigdsc->xecute_str.mvtype = MV_STR;
	return 0;	/* Could return ERR_TPRETRY if return is via our condition handler */
}

/* Workhorse of fetching source for given trigger.
 */
STATICFNDEF void trigger_fill_xecute_buffer_read_trigger_source(gv_trigger_t *trigdsc)
{
	mname_entry		gvent;
	enum cdb_sc		cdb_status;
	int4			index;
	mstr			gbl, xecute_buff;
	mval			trig_index;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	gvt_trigger_t		*gvt_trigger;
	gv_namehead		*gvt;
	gv_namehead		*hasht_tree, *save_gv_target;
	gv_key			*save_gv_currkey;
	gd_region		*save_gv_cur_region;
	sgm_info		*save_sgm_info_ptr;
	char			save_currkey[SIZEOF(gv_key) + DBKEYSIZE(MAX_KEY_SZ)];

	assert(0 < dollar_tlevel);
	assert(NULL != trigdsc);
	SAVE_TRIGGER_REGION_INFO;

	gvt_trigger = trigdsc->gvt_trigger;			/* We now know our base block now */
	index = trigdsc - gvt_trigger->gv_trig_array + 1;	/* We now know our trigger index value */
	i2mval(&trig_index, index);
	gvt = gv_target = gvt_trigger->gv_target;		/* gv_target contains global name */
	gbl.addr = gvt->gvname.var_name.addr;
	gbl.len = gvt->gvname.var_name.len;
	/* Our situation is that while our desired gv_target has csa information, we don't know specifically
	 * which global directory was in use so we can't run gv_bind_name() lest we find the given global
	 * name in the wrong global directory thus running the wrong triggers. But we know this target is
	 * properly formed since it had to be when it was recorded when the triggers were loaded. Because of
	 * that, we can get the correct csa and gv_target and csa-region will point us to a region that will
	 * work even if it isn't exactly the one we used to get to this trigger.
	 */
	TP_CHANGE_REG_IF_NEEDED(gvt->gd_csa->region);
	csa = cs_addrs;
	csd = csa->hdr;
	assert(csd == cs_data);
	tp_set_sgm();
	/* See if we need to reload our triggers */
	if ((csa->db_trigger_cycle != gvt->db_trigger_cycle)
	    || (csa->db_dztrigger_cycle && (gvt->db_dztrigger_cycle != csa->db_dztrigger_cycle)))
	{       /* The process' view of the triggers could be potentially stale. Restart to be safe.
		 * Triggers can be invoked only by GT.M and Update process. Out of these, we expect only
		 * GT.M to see restarts due to concurrent trigger changes. Update process is the only
		 * updater on the secondary so we dont expect it to see any concurrent trigger changes
		 * Assert accordingly.
		 */
		assert(CDB_STAGNATE > t_tries);
		assert(IS_GTM_IMAGE);
		t_retry(cdb_sc_triggermod);
	}
	SETUP_TRIGGER_GLOBAL;
	INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;
	assert(0 == trigdsc->xecute_str.str.len);	/* Make sure not replacing/losing a buffer */
	xecute_buff.addr = trigger_gbl_fill_xecute_buffer(gbl.addr, gbl.len, &trig_index, NULL, (int4 *)&xecute_buff.len);
	trigdsc->xecute_str.str = xecute_buff;
	/* Restore gv_target/gv_currkey which need to be kept in sync */
	RESTORE_TRIGGER_REGION_INFO;
	return;
}
#endif /* GTM_TRIGGER */
