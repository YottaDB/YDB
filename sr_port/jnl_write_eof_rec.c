/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
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
#include "jnl_get_checksum.h"

GBLREF  jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF 	jnl_gbls_t		jgbl;

void	jnl_write_eof_rec(sgmnt_addrs *csa, struct_jrec_eof *eof_record)
{
	jnl_private_control	*jpc;

	assert(csa->now_crit);
	jpc = csa->jnl;
	assert(0 != jpc->pini_addr);
	eof_record->prefix.jrec_type = JRT_EOF;
	eof_record->prefix.forwptr = eof_record->suffix.backptr = EOF_RECLEN;
	eof_record->suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	eof_record->prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	eof_record->prefix.tn = csa->hdr->trans_hist.curr_tn;
	eof_record->prefix.checksum = INIT_CHECKSUM_SEED;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	eof_record->prefix.time = jgbl.gbl_jrec_time;
	ASSERT_JNL_SEQNO_FILEHDR_JNLPOOL(csa->hdr, jnlpool_ctl); /* debug-only sanity check between seqno of filehdr and jnlpool */
	if (!jgbl.forw_phase_recovery)
	{
		if (REPL_ALLOWED(csa))
			eof_record->jnl_seqno = csa->hdr->reg_seqno;/* Note we cannot use jnlpool_ctl->jnl_seqno since
									      * we might not presently hold the journal pool lock */
		else
			eof_record->jnl_seqno = 0;
	} else
		QWASSIGN(eof_record->jnl_seqno, jgbl.mur_jrec_seqno);
	jnl_write(jpc, JRT_EOF, (jnl_record *)eof_record, NULL, NULL);
}
