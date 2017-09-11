/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"

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
#include "repl_msg.h"
#include "gtmsource.h"
#include "iosp.h"
#include "jnl_get_checksum.h"
#ifdef DEBUG
#include "jnl_typedef.h"
#endif

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	seq_num			seq_num_zero;
GBLREF	uint4			process_id;

void	jnl_write_ztp_logical(sgmnt_addrs *csa, jnl_format_buffer *jfb, uint4 com_csum, seq_num jnl_seqno)
{
	struct_jrec_upd		*jrec;
	jnl_private_control	*jpc;
	boolean_t		in_phase2;

	/* If REPL_WAS_ENABLED(csa) is TRUE, then we would not have gone through the code that initializes
	 * jgbl.gbl_jrec_time or jpc->pini_addr. But in this case, we are not writing the journal record
	 * to the journal buffer or journal file but write it only to the journal pool from where it gets
	 * sent across to the update process that does not care about these fields so it is ok to leave them as is.
	 */
	jpc = csa->jnl;
	assert((0 != jpc->pini_addr) || REPL_WAS_ENABLED(csa));
	assert(jgbl.gbl_jrec_time || REPL_WAS_ENABLED(csa));
	assert(IS_SET_KILL_ZKILL_ZTRIG(jfb->rectype));
	assert(IS_ZTP(jfb->rectype));
	jrec = (struct_jrec_upd *)jfb->buff;
	jrec->prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	in_phase2 = IN_PHASE2_JNL_COMMIT(csa);
	jrec->prefix.tn = JB_CURR_TN_APPROPRIATE(in_phase2, jpc, csa);
	jrec->prefix.time = jgbl.gbl_jrec_time;
	assert(0 != jnl_fence_ctl.token);
	jrec->token_seq.token = jnl_fence_ctl.token;
	jrec->strm_seqno = 0;	/* strm_seqno is only for replication & ZTCOM does not work with replic */
	COMPUTE_LOGICAL_REC_CHECKSUM(jfb->checksum, jrec, com_csum, jrec->prefix.checksum);
	assert(!REPL_ALLOWED(csa));
	JNL_WRITE_APPROPRIATE(csa, jpc, jfb->rectype, (jnl_record *)jrec, jfb);
}
