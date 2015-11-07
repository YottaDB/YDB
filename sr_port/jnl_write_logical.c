/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

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
GBLREF	uint4			dollar_tlevel;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	seq_num			seq_num_zero;

/* This called for TP and non-TP, but not for ZTP */
void	jnl_write_logical(sgmnt_addrs *csa, jnl_format_buffer *jfb, uint4 com_csum)
{
	struct_jrec_upd		*jrec;
	struct_jrec_null	*jrec_null;
	GTMCRYPT_ONLY(
		struct_jrec_upd	*jrec_alt;
	)
	jnl_private_control	*jpc;
	/* If REPL_WAS_ENABLED(csa) is TRUE, then we would not have gone through the code that initializes
	 * jgbl.gbl_jrec_time or jpc->pini_addr. But in this case, we are not writing the journal record
	 * to the journal buffer or journal file but write it only to the journal pool from where it gets
	 * sent across to the update process that does not care about these fields so it is ok to leave them as is.
	 */
	jpc = csa->jnl;
	assert((0 != jpc->pini_addr) || REPL_WAS_ENABLED(csa));
	assert(jgbl.gbl_jrec_time || REPL_WAS_ENABLED(csa));
	assert(csa->now_crit);
	assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(jfb->rectype) || (JRT_NULL == jfb->rectype));
	assert(!IS_ZTP(jfb->rectype));
	jrec = (struct_jrec_upd *)jfb->buff;
	assert(OFFSETOF(struct_jrec_null, prefix) == OFFSETOF(struct_jrec_upd, prefix));
	assert(SIZEOF(jrec_null->prefix) == SIZEOF(jrec->prefix));
	jrec->prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	jrec->prefix.tn = csa->ti->curr_tn;
	jrec->prefix.time = jgbl.gbl_jrec_time;
	/* t_end/tp_tend/mur_output_record has already set token/jnl_seqno into jnl_fence_ctl.token */
	assert((0 != jnl_fence_ctl.token) || (!dollar_tlevel && !jgbl.forw_phase_recovery && !REPL_ENABLED(csa))
		|| (!dollar_tlevel && jgbl.forw_phase_recovery && (repl_open != csa->hdr->intrpt_recov_repl_state)));
	assert(OFFSETOF(struct_jrec_null, jnl_seqno) == OFFSETOF(struct_jrec_upd, token_seq));
	assert(SIZEOF(jrec_null->jnl_seqno) == SIZEOF(jrec->token_seq));
	jrec->token_seq.token = jnl_fence_ctl.token;
	assert(OFFSETOF(struct_jrec_null, strm_seqno) == OFFSETOF(struct_jrec_upd, strm_seqno));
	assert(SIZEOF(jrec_null->strm_seqno) == SIZEOF(jrec->strm_seqno));
	jrec->strm_seqno = jnl_fence_ctl.strm_seqno;
	/*update checksum below*/
	if(JRT_NULL != jrec->prefix.jrec_type)
	{
		COMPUTE_LOGICAL_REC_CHECKSUM(jfb->checksum, jrec, com_csum, jrec->prefix.checksum);
	}
	else
		jrec->prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)jrec, SIZEOF(struct_jrec_null));

#	ifdef GTM_CRYPT
	if (csa->hdr->is_encrypted && REPL_ALLOWED(csa))
	{
		jrec_alt = (struct_jrec_upd *)jfb->alt_buff;
		jrec_alt->prefix = jrec->prefix;
		jrec_alt->token_seq = jrec->token_seq;
		jrec_alt->strm_seqno = jrec->strm_seqno;
		jrec_alt->num_participants = jrec->num_participants;
	}
#	endif
	JNL_WRITE_APPROPRIATE(csa, jpc, jfb->rectype, (jnl_record *)jrec, NULL, jfb);
}
