/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "jnl_get_checksum.h"

GBLREF 	jnl_gbls_t		jgbl;

void	jnl_write_pfin(sgmnt_addrs *csa)
{
	struct_jrec_pfin	pfin_record;
	jnl_private_control	*jpc;

	assert(!IN_PHASE2_JNL_COMMIT(csa));
	jpc = csa->jnl;
	assert((0 != jpc->pini_addr) ||
			(gtm_white_box_test_case_enabled && (WBTEST_JNL_FILE_LOST_DSKADDR == gtm_white_box_test_case_number)));
	pfin_record.prefix.jrec_type = JRT_PFIN;
	pfin_record.prefix.forwptr = pfin_record.suffix.backptr = PFIN_RECLEN;
	pfin_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	pfin_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	pfin_record.prefix.tn = csa->ti->curr_tn;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	pfin_record.prefix.time = jgbl.gbl_jrec_time;
	pfin_record.prefix.checksum = INIT_CHECKSUM_SEED;
	pfin_record.filler = 0;
	pfin_record.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (unsigned char *)&pfin_record, SIZEOF(struct_jrec_pfin));
	jnl_write(jpc, JRT_PFIN, (jnl_record *)&pfin_record, NULL);
}
