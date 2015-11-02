/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif
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
#include "jnl_get_checksum.h"

GBLREF  jnl_gbls_t      	jgbl;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;

void    jnl_write_trunc_rec(sgmnt_addrs *csa, uint4 orig_total_blks, uint4 orig_free_blocks, uint4 total_blks_after_trunc)
{
	struct_jrec_trunc	trunc_rec;
	jnl_private_control	*jpc;

	assert(csa->now_crit);
	jpc = csa->jnl;
	trunc_rec.prefix.jrec_type = JRT_TRUNC;
	trunc_rec.prefix.forwptr = trunc_rec.suffix.backptr = TRUNC_RECLEN;
	trunc_rec.prefix.tn = csa->ti->curr_tn;
	trunc_rec.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	trunc_rec.prefix.time = jgbl.gbl_jrec_time;
	trunc_rec.prefix.checksum = INIT_CHECKSUM_SEED;
	trunc_rec.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	trunc_rec.orig_total_blks = orig_total_blks;
	trunc_rec.orig_free_blocks = orig_free_blocks;
	trunc_rec.total_blks_after_trunc = total_blks_after_trunc;
	trunc_rec.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&trunc_rec, SIZEOF(struct_jrec_trunc));
	jnl_write(jpc, JRT_TRUNC, (jnl_record *)&trunc_rec, NULL, NULL);
}

