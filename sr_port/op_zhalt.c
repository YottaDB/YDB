/****************************************************************
 *								*
 * Copyright (c) 2011-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtmimagename.h"
#include "send_msg.h"
#include "getzposition.h"
#include "mvalconv.h"
#include "op.h"
#include "error.h"
#include "stringpool.h"
#include "restrict.h"
#include "gt_timer.h"
#include "error.h"
#include "gtmmsg.h"
#include "create_fatal_error_zshow_dmp.h"
#include "stack_frame.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gv_trigger.h"
#include "gtm_trigger.h"

GBLREF	int		mumps_status;
GBLREF	int		process_exiting;
GBLREF	int4		exi_condition;
GBLREF	stack_frame	*frame_pointer;

LITREF	gtmImageName	gtmImageNames[];

STATICDEF	ABS_TIME	halt_time;
STATICDEF	ABS_TIME	zhalt_time;

#define		SAFE_INTERVAL	500000		/* microseconds */

error_def(ERR_PROCTERM);
error_def(ERR_RESTRICTEDOP);

/* Exit process with a return code, either given or defaulted by the ZHALT or implicitly 0 for HALT */
void op_zhalt(int4 retcode, boolean_t is_zhalt)
{
	ABS_TIME		cur_time, interval;
	GTMTRIG_ONLY(mval	zposition;)
	mval			tmpmval, *tmpmv;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((0 != retcode) && (0 == (retcode & MAX_INT_IN_BYTE)))
		retcode = MAX_INT_IN_BYTE;	/* If the truncated return code is 0, make it 255 so a non-zero
						 * return code is returned instead if needed to the parent process.
						 */
	if (0 < TREF(gtmci_nested_level))
	{	/* Need to return to caller - not halt (halting out of this call-in level) */
		mumps_status = SUCCESS;
		if (is_zhalt)
		{	/* Set retcode in TREF(gtmci_retval) (needed later by "goframes" as part of "op_zg1" call below) */
			tmpmv = &tmpmval;
			i2mval(tmpmv, retcode);
			MV_FORCE_STR(tmpmv);
			TREF(gtmci_retval) = tmpmv;
			DBG_MARK_STRINGPOOL_UNUSABLE;	/* No GCs expected between now and when this is fetched in goframes() */
		}
		op_zg1(0);			/* Unwind everything back to beginning of this call-in level */
		/* The "op_zg1" would not return in most cases. An exception is if this call-in environment was created by
		 * simpleAPI and a trigger was invoked by say a "ydb_set_s" call and an error occured inside the trigger
		 * environment which resulted in an error trap that did a "halt/zhalt". In this case, the current frame_pointer
		 * would be a trigger base frame that needs to be unwound using special code (see goframes.c for similar code).
		 */
		assert(SFT_TRIGR & frame_pointer->type);
		if (SFT_TRIGR & frame_pointer->type)
			gtm_trigger_fini(TRUE, TRUE);
		/* Assert that we unwound to a call-in base frame */
		assert(SFT_CI & frame_pointer->type);
		/* We need to return to "ydb_ci[p]". Use MUM_TSTART */
		MUM_TSTART;
		assertpro(FALSE);		/* Should not return */
		return;				/* Previous call does not return so this is for the compiler */
	}
	if (IS_GTM_IMAGE && !(is_zhalt ? RESTRICTED(zhalt_op) : RESTRICTED(halt_op)))
		EXIT(is_zhalt ? retcode : 0);
	if (is_zhalt ? RESTRICTED(zhalt_op) : retcode ? FALSE : RESTRICTED(halt_op))
	{	/* if the operation is restricted and not from op_dmode or dm_read proclaim the restriction */
		sys_get_curr_time(&cur_time);
		if ((is_zhalt ? ((zhalt_time.at_usec) || (zhalt_time.at_sec)) : ((halt_time.at_usec) || (halt_time.at_sec))))
			interval = sub_abs_time(&cur_time, is_zhalt ? &zhalt_time : &halt_time);
		else
			interval.at_usec = interval.at_sec = 0;
		if ((interval.at_sec) || (0 == interval.at_usec) || (SAFE_INTERVAL < interval.at_usec))
		{
			if (is_zhalt)
				zhalt_time = cur_time;
			else
				halt_time = cur_time;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, is_zhalt ? "ZHALT" : "HALT");
		}
		/* if 2nd in less than SAFE_iNTERVAL, give FATAL message & YDB_FATAL file, but no core, to stop nasty loops */
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_SEVERE(ERR_RESTRICTEDOP), 1, is_zhalt ? "ZHALT" : "HALT");
		if (IS_GTM_IMAGE)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_SEVERE(ERR_RESTRICTEDOP), 1, is_zhalt ? "ZHALT" : "HALT");
			exi_condition = ERR_RESTRICTEDOP;
		}
	} else
		exi_condition = retcode;	/* in case it's not MUPIP send it out with it's original retcode */
#	ifdef GTM_TRIGGER
	if (IS_MUPIP_IMAGE)
	{	/* MUPIP (Update Server) always goes out leaving a context file */
		exi_condition = ERR_PROCTERM;
		zposition.mvtype = 0;		/* It's not an mval yet till getzposition fills it in */
		getzposition(&zposition);
		assert(MV_IS_STRING(&zposition) && (0 < zposition.str.len));
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_PROCTERM, 6, GTMIMAGENAMETXT(image_type),
			is_zhalt ? "ZHALT" : "HALT", retcode, zposition.str.len, zposition.str.addr);
	} else
		assert(IS_GTM_IMAGE);
#	endif
	process_exiting = TRUE;			/* do a little dance to leave a YDB_FATAL file but no core file */
	if (retcode && !is_zhalt)
		EXIT(0); 			/* unless this is a op_dmode or dm_read exit */
	create_fatal_error_zshow_dmp(MAKE_MSG_SEVERE(ERR_RESTRICTEDOP));
	exi_condition = 0;
	stop_image_no_core();
}
