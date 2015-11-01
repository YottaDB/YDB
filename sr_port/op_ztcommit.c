/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_time.h"

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "op.h"
#include "jnl_write.h"
#include "wcs_timer_start.h"

error_def(ERR_TRANSMINUS);
error_def(ERR_TRANSNOSTART);

GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF  jnl_fence_control       jnl_fence_ctl;
GBLREF  short                   dollar_tlevel;
GBLREF  uint4                   zts_jrec_time;
GBLREF	boolean_t		copy_jnl_record;
GBLREF	struct_jrec_tcom 	mur_jrec_fixed_tcom;

void    op_ztcommit(int4 n)
{
        struct_jrec_tcom 	ztcom_record;
        sgmnt_addrs             *csa, *next_csa;
	uint4			tc_jrec_time;

        if (n < 0)
                rts_error(VARLSTCNT(1) ERR_TRANSMINUS);
        if (jnl_fence_ctl.level == 0  ||  n > jnl_fence_ctl.level)
                rts_error(VARLSTCNT(1) ERR_TRANSNOSTART);
        assert(jnl_fence_ctl.level > 0);
        assert(dollar_tlevel == 0);

        if (n == 0)
                jnl_fence_ctl.level = 0;
        else
                jnl_fence_ctl.level -= n;

        if (jnl_fence_ctl.level == 0)
        {
                ztcom_record.token = jnl_fence_ctl.token;
                ztcom_record.participants = jnl_fence_ctl.region_count;

                /* Note that only those regions that are actively journaling will appear in the following list: */
                for (csa = jnl_fence_ctl.fence_list;  csa != (sgmnt_addrs *) - 1;  csa = csa->next_fenced)
                {
                        grab_crit(csa->jnl->region);
			JNL_SHORT_TIME(tc_jrec_time);	/* get current time after holding crit on the region */
			if (!copy_jnl_record)
			{
				ztcom_record.pini_addr = csa->jnl->pini_addr;
				ztcom_record.ts_short_time = zts_jrec_time;
				ztcom_record.tc_short_time = tc_jrec_time;
			} else
			{
				ztcom_record.pini_addr = mur_jrec_fixed_tcom.pini_addr;
				ztcom_record.ts_short_time = mur_jrec_fixed_tcom.ts_short_time;
				ztcom_record.tc_short_time = mur_jrec_fixed_tcom.tc_short_time;
			}
			ztcom_record.ts_recov_short_time = zts_jrec_time;
			ztcom_record.tc_recov_short_time = tc_jrec_time;
                        ztcom_record.tn = csa->ti->curr_tn;
			QWASSIGN(ztcom_record.jnl_seqno, temp_jnlpool_ctl->jnl_seqno);
                        jnl_write(csa->jnl, JRT_ZTCOM, (jrec_union *)&ztcom_record, NULL, NULL);
                        rel_crit(csa->jnl->region);
                        wcs_timer_start(csa->jnl->region, TRUE);
                }
                for (csa = jnl_fence_ctl.fence_list;  csa != (sgmnt_addrs *) - 1;  csa = next_csa)
                {       /* do the waits in a separate loop to prevent spreading out the transaction */
                        jnl_wait(csa->jnl->region);
                        next_csa = csa->next_fenced;
                        csa->next_fenced = NULL;
                }
        }
}
