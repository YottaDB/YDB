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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"
#include "jnl_get_checksum.h"

void	jnl_write_align_rec(sgmnt_addrs *csa, uint4 align_filler_len, jnl_tm_t time)
{
	struct_jrec_align	align_rec;
	jnl_private_control	*jpc;
	uint4			align_rec_len;

	/* The below assert is the reason why we have a GET_JREC_CHECKSUM macro */
	assert(OFFSETOF(struct_jrec_align, checksum) != OFFSETOF(jrec_prefix, checksum));
	assert(OFFSETOF(struct_jrec_align, checksum) == OFFSETOF(jrec_prefix, pini_addr));
	assert(SIZEOF(align_rec.checksum) == SIZEOF(((jrec_prefix *)NULL)->checksum));
	assert(OFFSETOF(struct_jrec_align, time) == OFFSETOF(jrec_prefix, time));
	assert(SIZEOF(align_rec.time) == SIZEOF(((jrec_prefix *)NULL)->time));
	align_rec_len = MIN_ALIGN_RECLEN + align_filler_len;
	jpc = csa->jnl;
	assert(align_rec_len <= jpc->jnl_buff->max_jrec_len);
	assert(0 == (align_rec_len % JNL_REC_START_BNDRY));
	align_rec.jrec_type = JRT_ALIGN;
	align_rec.forwptr = align_rec_len;
	align_rec.time = time;
	align_rec.checksum = INIT_CHECKSUM_SEED;
	align_rec.checksum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)&align_rec, FIXED_ALIGN_RECLEN);
	jnl_write(jpc, JRT_ALIGN, (jnl_record *)&align_rec, NULL);
}
