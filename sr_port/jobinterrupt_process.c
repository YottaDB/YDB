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

#include "gtm_string.h"

#include "error.h"
#include "indir_enum.h"
#include <rtnhdr.h>
#include "op.h"
#include "stack_frame.h"
#include "error_trap.h"
#include "mv_stent.h"
#include "gtm_stdio.h"
#include "stringpool.h"
#include "jobinterrupt_process.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashtab_int4.h"
#include "gdskill.h"
#include "jnl.h"
#include "gdscc.h"
#include "buddy_list.h"
#include "tp.h"
#include "gvname_info.h"
#include "op_merge.h"
#include "zwrite.h"
#include "zshow.h"

GBLREF stack_frame		*frame_pointer, *error_frame;
GBLREF unsigned char		*stackbase, *stacktop, *msp, *stackwarn;
GBLREF spdesc			stringpool;
GBLREF mval			dollar_zinterrupt;
GBLREF boolean_t		dollar_zininterrupt;
GBLREF unsigned short		proc_act_type;
GBLREF mv_stent			*mv_chain;
GBLREF int			dollar_truth;
GBLREF mstr			extnam_str;
GBLREF unsigned char		*restart_pc, *restart_ctxt;
GBLREF dollar_ecode_type	dollar_ecode;
GBLREF dollar_stack_type	dollar_stack;
GBLREF int			merge_args;
GBLREF uint4			zwrtacindx;
GBLREF merge_glvn_ptr		mglvnp;
GBLREF gvzwrite_datablk		*gvzwrite_block;
GBLREF lvzwrite_datablk		*lvzwrite_block;
GBLREF zshow_out		*zwr_output;
GBLREF zwr_hash_table		*zwrhtab;

error_def(ERR_STACKOFLOW);
error_def(ERR_STACKCRIT);

void jobinterrupt_process(void)
{
	mv_stent	*mv_st_ent;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(dollar_zininterrupt);
	/* Compile and push new (counted) frame onto the stack to drive the
	 * $zinterrupt handler.
	 */
	assert((SFT_COUNT | SFT_ZINTR) == proc_act_type);
	op_commarg(&dollar_zinterrupt, indir_linetail);
	frame_pointer->type = proc_act_type;	/* The mark of zorro.. */
	proc_act_type = 0;
	/* Save restart_pc/ctxt so a resumed frame or ztrap can resume in the corrct place and
	 * not and inappropriate resume point determined by the interrupting code.
	 */
	PUSH_MV_STENT(MVST_RSTRTPC);
	mv_st_ent = mv_chain;
	mv_st_ent->mv_st_cont.mvs_rstrtpc.restart_pc_save = restart_pc;
	mv_st_ent->mv_st_cont.mvs_rstrtpc.restart_ctxt_save = restart_ctxt;
	/* Now we need to preserve our current environment. This MVST_ZINTR mv_stent type will hold
	 * the items deemed necessary to preserve. All other items are the user's responsibility.
	 *
	 * Initialize the mv_stent elements processed by stp_gcol which can be called for either the
	 * op_gvsavtarg() or extnam items. This initialization keeps stp_gcol from attempting to
	 * process unset fields with garbage in them as valid mstr address/length pairs.
	 */
	PUSH_MV_STENT(MVST_ZINTR);
	mv_st_ent = mv_chain;
	mv_st_ent->mv_st_cont.mvs_zintr.savtarg.str.len = 0;
	mv_st_ent->mv_st_cont.mvs_zintr.savextref.len = 0;
	mv_st_ent->mv_st_cont.mvs_zintr.saved_dollar_truth = dollar_truth;
	op_gvsavtarg(&mv_st_ent->mv_st_cont.mvs_zintr.savtarg);
	if (extnam_str.len)
	{
		ENSURE_STP_FREE_SPACE(extnam_str.len);
		mv_st_ent->mv_st_cont.mvs_zintr.savextref.addr = (char *)stringpool.free;
		memcpy(mv_st_ent->mv_st_cont.mvs_zintr.savextref.addr, extnam_str.addr, extnam_str.len);
		stringpool.free += extnam_str.len;
		assert(stringpool.free <= stringpool.top);
	}
	mv_st_ent->mv_st_cont.mvs_zintr.savextref.len = extnam_str.len;
	/* save/restore $ECODE/$STACK over this invocation */
	mv_st_ent->mv_st_cont.mvs_zintr.error_frame_save = error_frame;
	memcpy(&mv_st_ent->mv_st_cont.mvs_zintr.dollar_ecode_save, &dollar_ecode, SIZEOF(dollar_ecode));
	memcpy(&mv_st_ent->mv_st_cont.mvs_zintr.dollar_stack_save, &dollar_stack, SIZEOF(dollar_stack));
	NULLIFY_ERROR_FRAME;
	ecode_init();
	/* If we interrupted a Merge, ZWrite, or ZShow, save the state info in an mv_stent that will be restored when this
	 * interrupt frame returns. Note that at this time, return from an interrupt does not "return" to the interrupt
	 * point but rather restarts the line of M code we were running *OR* at the most recent save point (set by
	 * op_restartpc or equivalent). In the future this is likely to change, at least for an interrupted Merge, ZWrite
	 * or ZShow command, so this save/restore is appropriate both now (to let these nest at all) and especially in the future.
	 */
	if (TREF(in_zwrite) || (0 != merge_args))
		PUSH_MVST_MRGZWRSV;
	return;
}
