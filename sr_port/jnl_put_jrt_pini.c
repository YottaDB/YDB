/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnl_process_vector	*prc_vec;
GBLREF	jnl_process_vector	*originator_prc_vec;
GBLREF 	jnl_gbls_t		jgbl;

void	jnl_put_jrt_pini(sgmnt_addrs *csa)
{
	struct_jrec_pini	pini_record;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	struct pini_list	*mur_plst;

	assert(csa->now_crit);
	jpc = csa->jnl;
	jbp = jpc->jnl_buff;
	assert(prc_vec);
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	pini_record.prefix.jrec_type = JRT_PINI;
	pini_record.prefix.forwptr = pini_record.suffix.backptr = PINI_RECLEN;
	pini_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	pini_record.prefix.pini_addr = jbp->freeaddr;
	/* in case an ALIGN record is written before the PINI record in jnl_write(), pini_addr above is updated appropriately. */
	pini_record.prefix.tn = csa->ti->curr_tn;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	pini_record.prefix.time = jgbl.gbl_jrec_time;
	JNL_WHOLE_FROM_SHORT_TIME(prc_vec->jpv_time, jgbl.gbl_jrec_time);
	pini_record.prefix.checksum = INIT_CHECKSUM_SEED;
	/* Note that only pini_record.prefix.time is considered in mupip journal command processing.
	 * prc_vec->jpv_time is for accounting purpose only. Usually it is kind of redundant too. */
	if (!jgbl.forw_phase_recovery)
	{
		assert(NULL == jgbl.mur_pini_addr_reset_fnptr);
		assert(NULL == csa->rctl);
		mur_plst = NULL;
		if (IS_GTCM_GNP_SERVER_IMAGE && (NULL != originator_prc_vec))
		{
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
				(unsigned char*)originator_prc_vec, SIZEOF(jnl_process_vector));
		} else
			memset((unsigned char*)&pini_record.process_vector[ORIG_JPV], 0, SIZEOF(jnl_process_vector));
	} else
	{
		assert(NULL != csa->rctl);
		mur_plst = csa->rctl->mur_plst;
		if (NULL != jgbl.mur_pini_addr_reset_fnptr)
		{
			memcpy((unsigned char*)&pini_record.process_vector[ORIG_JPV],
					(unsigned char *)&mur_plst->origjpv, SIZEOF(jnl_process_vector));
		} else
		{	/* gdsfilext done during "mur_block_count_correct" */
			assert(NULL == mur_plst);
			memset((unsigned char*)&pini_record.process_vector[ORIG_JPV], 0, SIZEOF(jnl_process_vector));
		}
	}
	memcpy((unsigned char*)&pini_record.process_vector[CURR_JPV], (unsigned char*)prc_vec, SIZEOF(jnl_process_vector));
	pini_record.filler = 0;
	pini_record.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&pini_record, SIZEOF(struct_jrec_pini));
	jnl_write(jpc, JRT_PINI, (jnl_record *)&pini_record, NULL, NULL);
	/* Note : jpc->pini_addr should not be updated until PINI record is written [C9D08-002376] */
	jpc->pini_addr = jbp->freeaddr - PINI_RECLEN;
	assert(jgbl.forw_phase_recovery || (NULL == mur_plst));
	if (NULL != mur_plst)
		mur_plst->new_pini_addr = jpc->pini_addr;/* note down for future forward play logical record processing */
}
