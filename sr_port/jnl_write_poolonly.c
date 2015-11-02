/****************************************************************
 *								*
 *	Copyright 2007, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "gtm_inet.h"

#include <stddef.h> /* for offsetof() macro */

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
#include "ccp.h"
#include "iosp.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "min_max.h"
#include "sleep_cnt.h"
#include "jnl_write.h"
#include "copy.h"

GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;

GBLREF	uint4			process_id;
GBLREF	sm_uc_ptr_t		jnldata_base;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	jnl_gbls_t		jgbl;

/* This function does a subset of what "jnl_write" does. While "jnl_write" writes the journal record to the journal buffer,
 * journal file and journal pool, this function writes the journal records ONLY TO the journal pool. This function should
 * be invoked only if replication state is WAS_ON (repl_was_open) and journaling state is jnl_closed.
 *
 * jpc 	   : Journal private control
 * rectype : Record type
 * jnl_rec : This contains fixed part of a variable size record or the complete fixed size records.
 * jfb     : For SET/KILL/ZKILL records entire record is formatted in this.
 */
void	jnl_write_poolonly(jnl_private_control *jpc, enum jnl_record_type rectype, jnl_record *jnl_rec, jnl_format_buffer *jfb)
{
	int4			align_rec_len, rlen, rlen_with_align, srclen, dstlen;
	jnl_buffer_ptr_t	jb;
	sgmnt_addrs		*csa;
	struct_jrec_align	align_rec;
	uint4 			status;
	jrec_suffix		suffix;
	boolean_t		nowrap;
	struct_jrec_blk		*jrec_blk;
	uint4			jnlpool_size;
	uchar_ptr_t		jnlrecptr;
	DEBUG_ONLY(uint4	lcl_dskaddr;)
	uchar_ptr_t		tmp_buff;

	error_def(ERR_JNLWRTNOWWRTR);
	error_def(ERR_JNLWRTDEFER);

	assert(NULL != jnl_rec);
	assert(rectype > JRT_BAD  &&  rectype < JRT_RECTYPES && JRT_ALIGN != rectype);
	assert(jrt_is_replicated[rectype]);
	assert((NULL != jnlpool.jnlpool_ctl) && (NULL != jnlpool_ctl)); /* ensure we haven't yet detached from the jnlpool */
	assert((&FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs)->now_crit);	/* ensure we have the jnl pool lock */
	csa = &FILE_INFO(jpc->region)->s_addrs;
	assert(!JNL_ENABLED(csa) && REPL_WAS_ENABLED(csa));
	assert(csa->now_crit || (csa->hdr->clustered  &&  csa->nl->ccp_state == CCST_CLOSED));
	jb = jpc->jnl_buff;
	++jb->reccnt[rectype];
	rlen = jnl_rec->prefix.forwptr;
	assert(0 == rlen % JNL_REC_START_BNDRY);
	jb->bytcnt += rlen;
	DEBUG_ONLY(jgbl.cu_jnl_index++;)
	jnlpool_size = temp_jnlpool_ctl->jnlpool_size;
	dstlen = jnlpool_size - temp_jnlpool_ctl->write;
	if (jrt_fixed_size[rectype])
		jnlrecptr = (uchar_ptr_t)jnl_rec;
#	ifdef GTM_CRYPT
	else if(csa->hdr->is_encrypted && IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
		jnlrecptr = (uchar_ptr_t)jfb->alt_buff;
#	endif
	else
		jnlrecptr = (uchar_ptr_t)jfb->buff;

	if (rlen <= dstlen)	/* dstlen & srclen >= rlen  (most frequent case) */
		memcpy(jnldata_base + temp_jnlpool_ctl->write, jnlrecptr, rlen);
	else			/* dstlen < rlen <= srclen */
	{
		memcpy(jnldata_base + temp_jnlpool_ctl->write, jnlrecptr, dstlen);
		memcpy(jnldata_base, jnlrecptr + dstlen, rlen - dstlen);
	}
	temp_jnlpool_ctl->write += rlen;
	if (temp_jnlpool_ctl->write >= jnlpool_size)
		temp_jnlpool_ctl->write -= jnlpool_size;
}
