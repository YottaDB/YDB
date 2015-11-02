/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "jnl_get_checksum.h"

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnl_process_vector	*prc_vec;
GBLREF	jnl_process_vector	*originator_prc_vec;
GBLREF	short			dollar_tlevel;
GBLREF	enum gtmImageTypes 	image_type;
GBLREF 	jnl_gbls_t		jgbl;

void	jnl_put_jrt_pini(sgmnt_addrs *csa)
{
	struct_jrec_pini	pini_record;

	assert(csa->now_crit);
	assert(prc_vec);
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	pini_record.prefix.jrec_type = JRT_PINI;
	pini_record.prefix.forwptr = pini_record.suffix.backptr = PINI_RECLEN;
	pini_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	pini_record.prefix.pini_addr = csa->jnl->jnl_buff->freeaddr;
	/* in case an ALIGN record is written before the PINI record in jnl_write(), pini_addr above is updated appropriately. */
	pini_record.prefix.tn = csa->ti->curr_tn;
	assert(jgbl.gbl_jrec_time);	/* the caller should have set it */
	if (!jgbl.gbl_jrec_time)
	{	/* no idea how this is possible, but just to be safe */
		JNL_SHORT_TIME(jgbl.gbl_jrec_time);
	}
	pini_record.prefix.time = jgbl.gbl_jrec_time;
	JNL_WHOLE_FROM_SHORT_TIME(prc_vec->jpv_time, jgbl.gbl_jrec_time);
	pini_record.prefix.checksum = INIT_CHECKSUM_SEED;
	/* Note that only pini_record.prefix.time is considered in mupip journal command processing.
	 * prc_vec->jpv_time is for accounting purpose only. Usually it is kind of redundant too. */
	if (!jgbl.forw_phase_recovery)
	{
		if (GTCM_GNP_SERVER_IMAGE == image_type && NULL != originator_prc_vec)
		{
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
				(unsigned char*)originator_prc_vec, sizeof(jnl_process_vector));
		} else
			memset((unsigned char*)&pini_record.process_vector[ORIG_JPV], 0, sizeof(jnl_process_vector));
	} else
	{
		if (NULL != jgbl.mur_plst)
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
					(unsigned char *)&jgbl.mur_plst->origjpv, sizeof(jnl_process_vector));
		else
		{	/* gdsfilext done during "mur_block_count_correct" */
			memset((unsigned char*)&pini_record.process_vector[ORIG_JPV], 0, sizeof(jnl_process_vector));
		}
	}
	memcpy((unsigned char*)&pini_record.process_vector[CURR_JPV], (unsigned char*)prc_vec, sizeof(jnl_process_vector));
	jnl_write(csa->jnl, JRT_PINI, (jnl_record *)&pini_record, NULL, NULL);
	/* Note : csa->jnl->pini_addr should not be updated until PINI record is written [C9D08-002376] */
	csa->jnl->pini_addr = csa->jnl->jnl_buff->freeaddr - PINI_RECLEN;
	if (jgbl.forw_phase_recovery && (NULL != jgbl.mur_plst))
		jgbl.mur_plst->new_pini_addr = csa->jnl->pini_addr;/* note down for future forward play logical record processing */
}
