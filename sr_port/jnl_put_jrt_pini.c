/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "gtm_time.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"
#include "gtmimagename.h"

GBLDEF	jnl_fence_control	jnl_fence_ctl;
GBLDEF	jnl_process_vector	*prc_vec = NULL;		/* for current process */
GBLDEF	jnl_process_vector	*originator_prc_vec = NULL;	/* for client/originator */
GBLDEF	jnl_process_vector	*server_prc_vec = NULL;		/* for gtcm server */

GBLREF	short			dollar_tlevel;
GBLREF	uint4			gbl_jrec_time;	/* see comment in gbldefs.c for usage */
GBLREF  uint4			cur_logirec_short_time;	/* see comment in gbldefs.c for usage */
GBLREF	enum gtmImageTypes 	image_type;
GBLREF  boolean_t               forw_phase_recovery;

LITREF	int			jnl_fixed_size[];

void	jnl_put_jrt_pini(sgmnt_addrs *csa)
{
	struct_jrec_pini	pini_record;

	assert(csa->now_crit);
	assert(prc_vec);
	/* t_end, tp_tend call jnl_put_jrt_pini first and later jnl_write_pblk. It's possible that we can get a pini time
	 * stamp greater than the later pblk as setting in pblk is based on the earlier gbl_jrec_time in t_end, tp_tend.
	 * For non-TP, it is possible that a non t_end/tp_tend routine (e.g. the routines that write an aimg record
	 * or an inctn record) calls this function with differing early_tn and curr_tn, in which case that need not
	 * have set gbl_jrec_time appropriately. We don't want to take a chance.
	 */
	if (!forw_phase_recovery) /* For GTM only set the time as gbl_jrec_time, else retain the original PINI short time */
	{
		if (dollar_tlevel && csa->ti->early_tn != csa->ti->curr_tn)
		{	/* in the commit phase of a transaction */
			assert(csa->ti->early_tn == csa->ti->curr_tn + 1);
			/* The JNL_WHOLE_TIME done below isn't necessary in Unix since it is anyway going to be overridden
			 * by the MID_TIME call one step later. In VMS, WHOLE_TIME fills in an 8-byte value while MID_TIME fills
			 * in only the middle 4-bytes hence both are necessary.
			 */
			VMS_ONLY(JNL_WHOLE_TIME(prc_vec->jpv_time);)
			MID_TIME(prc_vec->jpv_time) = gbl_jrec_time; /* Reset mid_time to correspond to gbl_jrec_time for GTM */
		} else
		{	/* For Non TP, it is possible that gbl_jrec_time is already set in t_end() to a timestamp value (which
			 * will be used while writing the PBLK record in t_end) and we get a later value for prc_vec->jpv_time
			 * (possible in rare timing situations) which results in PINI record getting written with a later
			 * timestamp than a later written PBLK record i.e. an out-of-order timestamp in the journal file.
			 * So reset gbl_jrec_time to be the latest, all later records use this time */
			VMS_ONLY(JNL_WHOLE_TIME(prc_vec->jpv_time);)
			VMS_ONLY(gbl_jrec_time = MID_TIME(prc_vec->jpv_time);)
			UNIX_ONLY(JNL_WHOLE_TIME(gbl_jrec_time);)
			UNIX_ONLY(prc_vec->jpv_time = gbl_jrec_time;)
		}
	}

	csa->jnl->regnum = ++jnl_fence_ctl.total_regions;
	memcpy((unsigned char*)&pini_record.process_vector[CURR_JPV], (unsigned char*)prc_vec, sizeof(jnl_process_vector));
	if (!forw_phase_recovery)
	{
		if (GTCM_GNP_SERVER_IMAGE == image_type)
		{
			assert(originator_prc_vec);
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
				(unsigned char*)originator_prc_vec, sizeof(jnl_process_vector));
			memcpy((unsigned char*)&pini_record.process_vector[SRVR_JPV],
				(unsigned char*)prc_vec, sizeof(jnl_process_vector));
		} else
		{
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
				(unsigned char*)prc_vec, sizeof(jnl_process_vector));
			memset((unsigned char*)&pini_record.process_vector[SRVR_JPV],
				0, sizeof(jnl_process_vector));
		}
	} else
	{
		if (NULL == originator_prc_vec)
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
				(unsigned char*)prc_vec, sizeof(jnl_process_vector));
		else
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
				(unsigned char*)originator_prc_vec, sizeof(jnl_process_vector));
		if (NULL == server_prc_vec)
			memset((unsigned char*)&pini_record.process_vector[SRVR_JPV], 0, sizeof(jnl_process_vector));
		else
			memcpy((unsigned char*)&pini_record.process_vector[SRVR_JPV],
				(unsigned char*)server_prc_vec, sizeof(jnl_process_vector));
		if (0 == pini_record.process_vector[SRVR_JPV].jpv_pid
			&& 0 ==  pini_record.process_vector[SRVR_JPV].jpv_image_count)
			cur_logirec_short_time = MID_TIME(pini_record.process_vector[ORIG_JPV].jpv_time);
		else
			cur_logirec_short_time = MID_TIME(pini_record.process_vector[SRVR_JPV].jpv_time);
	}
	jnl_write(csa->jnl, JRT_PINI, (jrec_union *)&pini_record, NULL, NULL);
	csa->jnl->pini_addr = csa->jnl->jnl_buff->freeaddr - JREC_PREFIX_SIZE - JREC_SUFFIX_SIZE - jnl_fixed_size[JRT_PINI];
}
