/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "jnl_typedef.h"
#ifdef DEBUG
#include "gdsbml.h"
#endif

#define	ADD_JREC_RSRV_ELEM(JPC, WRITE_TO_JNLBUFF, JRS, RECTYPE, RECLEN, PARAM1)				\
{													\
	jrec_rsrv_elem_t	*jre;									\
	uint4			alloclen, usedlen;							\
													\
	alloclen = JRS->alloclen;									\
	usedlen = JRS->usedlen;										\
	assert(usedlen <= alloclen);									\
	assert(usedlen || !JRS->tot_jrec_len);								\
	assert(!usedlen || JRS->tot_jrec_len);								\
	if (usedlen >= alloclen)									\
	{	/* Allocated space is not enough. Expand by allocating twice as much			\
		 * each time (to reduce # of expansion attempts).					\
		 */											\
		jre = JRS->jrs_array;									\
		alloclen = (!alloclen ? INIT_NUM_JREC_RSRV_ELEMS : alloclen * 2);			\
		JRS->jrs_array = (jrec_rsrv_elem_t *)malloc(alloclen * SIZEOF(jrec_rsrv_elem_t));	\
		JRS->alloclen = alloclen;								\
		if (NULL != jre)									\
		{											\
			memcpy(JRS->jrs_array, jre, usedlen * SIZEOF(jrec_rsrv_elem_t));		\
			free(jre);									\
		}											\
	}												\
	assert(usedlen < alloclen);									\
	jre = &JRS->jrs_array[usedlen];									\
	jre->rectype = RECTYPE;										\
	jre->reclen = RECLEN;										\
	jre->param1 = PARAM1;										\
	usedlen++;											\
	JRS->usedlen = usedlen;										\
	JRS->tot_jrec_len += RECLEN;									\
	assert(0 == (JRS->tot_jrec_len % JNL_REC_START_BNDRY));						\
	if (WRITE_TO_JNLBUFF)										\
		JPC->phase2_freeaddr += RECLEN;								\
}

/* This function reserves space in the journal buffer and the journal pool for the input journal record.
 * This is called when we hold crit from t_end/tp_tend. Once the callers release crit, they will invoke
 * the appropriate jnl_write_* function to copy the journal records from private buffers onto the reserved
 * space in shared memory. This way we minimize holding crit while doing the journal record copy.
 * If the database is in the WAS_ON state (REPL_WAS_ENABLED is TRUE), then the reservation happens only
 * in the journal pool, not in the journal buffers.
 */
void	jnl_write_reserve(sgmnt_addrs *csa, jbuf_rsrv_struct_t *jbuf_rsrv_ptr,
					enum jnl_record_type rectype, uint4 reclen, void *param1)
{
	boolean_t		write_to_jnlbuff;
	jnl_buffer_ptr_t	jbp;
	jnl_private_control	*jpc;
	uint4			align_reclen, lcl_freeaddr;
#	ifdef DEBUG
	boolean_t		write_to_jnlpool;
#	endif

	assert((JRT_PINI == rectype) || (JRT_PBLK == rectype) || (JRT_AIMG == rectype) || (JRT_INCTN == rectype)
		|| (JRT_TCOM == rectype) || IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype) || (JRT_NULL == rectype));
	assert(JNL_ALLOWED(csa));
	assert(csa->now_crit);
	assert(0 == (reclen % JNL_REC_START_BNDRY));
	jpc = csa->jnl;
	jbp = jpc->jnl_buff;
	assert(JNL_WRITE_LOGICAL_RECS(csa));
	write_to_jnlbuff = JNL_ENABLED(csa);
	DEBUG_ONLY(write_to_jnlpool = REPL_ALLOWED(csa);)
	assert(write_to_jnlbuff || write_to_jnlpool);
	if (write_to_jnlbuff)
	{
		lcl_freeaddr = jpc->phase2_freeaddr;
		if (lcl_freeaddr + reclen > jbp->next_align_addr)
		{
			assert(jbp->next_align_addr >= lcl_freeaddr);
			align_reclen = (jbp->next_align_addr - lcl_freeaddr) + MIN_ALIGN_RECLEN;
			assert(align_reclen < jbp->alignsize);
			ADD_JREC_RSRV_ELEM(jpc, write_to_jnlbuff, jbuf_rsrv_ptr, JRT_ALIGN, align_reclen, param1);
			jbp->next_align_addr += jbp->alignsize;
		}
	}
	ADD_JREC_RSRV_ELEM(jpc, write_to_jnlbuff, jbuf_rsrv_ptr, rectype, reclen, param1);
	/* Since this function reserves space for the input journal record in the current journal file, we expect
	 * no journal file switches to happen while writing all journal records of the current transaction in this
	 * region. If there was not enough space in the journal file, we expect t_end/tp_tend (callers of this function)
	 * to have done a "jnl_file_extend" as appropriate and if extension/autoswitch failed (due to permissions etc.)
	 * to have set jbp->last_eof_written to TRUE and transferred control to an error handler instead of proceeding
	 * with the transaction commit. So assert this field is FALSE. The only exception is if we are in the WAS_ON
	 * state in which case this function is invoked to only write journal records to the journal pool and not to
	 * the journal file.
	 */
	assert(!jbp->last_eof_written || !write_to_jnlbuff);
}
