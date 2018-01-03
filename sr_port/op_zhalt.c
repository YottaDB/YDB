/****************************************************************
 *								*
 * Copyright (c) 2011-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "restrict.h"
#include "gt_timer.h"
#include "error.h"
#include "gtmmsg.h"
#include "create_fatal_error_zshow_dmp.h"

GBLREF	int	process_exiting;
GBLREF	int4	exi_condition;
LITREF		gtmImageName	gtmImageNames[];

STATICDEF	ABS_TIME	halt_time;
STATICDEF	ABS_TIME	zhalt_time;

#define		SAFE_INTERVAL	500000		/* microseconds */

error_def(ERR_PROCTERM);
error_def(ERR_RESTRICTEDOP);

/* Exit process with a return code, either given or defaulted by the ZHALT or implicitly 0 for HALT */
void op_zhalt(int4 retcode, boolean_t is_zhalt)
{
	ABS_TIME        cur_time, interval;
	#	ifdef GTM_TRIGGER
	mval	zposition;
	#	endif

	if ((0 != retcode) && (0 == (retcode & MAX_INT_IN_BYTE)))
		retcode = MAX_INT_IN_BYTE;		/* If the truncated return code is 0, make it 255 in case it's returned
							 * to the parent process.
							 */
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
		/* if 2nd in less than SAFE_iNTERVAL, give FATAL message & GTM_FATAL file, but no core, to stop nasty loops */
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
	}
	else
		assert(IS_GTM_IMAGE);
	#	endif
	process_exiting = TRUE;			/* do a little dance to leave a GTM_FATAL file but no core file */
	if (retcode && !is_zhalt)
		EXIT(0); 			/* unless this is a op_dmode or dm_read exit */
	create_fatal_error_zshow_dmp(MAKE_MSG_SEVERE(ERR_RESTRICTEDOP));
	exi_condition = 0;
	stop_image_no_core();
}
