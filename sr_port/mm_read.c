/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "mm_read.h"
#include "gtmimagename.h"
#include "gds_blk_upgrade.h"
#include "gdsbml.h"
#include "filestruct.h"
#include "gtm_common_defs.h"
#include "memcoherency.h"
#include "gds_blk_upgrade_inline.h"

GBLREF	boolean_t		dse_running, is_updhelper, mu_reorg_encrypt_in_prog, mu_reorg_process;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs 		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	trans_num		start_tn;
GBLREF	uint4			mu_upgrade_in_prog;
GBLREF	unsigned char		rdfail_detail;

#define	MUPIP_UPGRADE_IN_PROGRESS	2	/* Copied from sr_port/mu_upgrade_bmm.c */

error_def(ERR_DBFILERDONLY);

DEFINE_ATOMIC_OP(gtm_atomic_ushort, ATOMIC_COMPARE_EXCHANGE, memory_order_relaxed, memory_order_relaxed)

sm_uc_ptr_t mm_read(block_id blk)
{	/* this is a kissing cousin to code in dsk_read and the two blocs should be maintained in parallel */
	boolean_t		read_only;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		buff;
	unsigned short		expected, newver;
	boolean_t		success;
#ifdef DEBUG
	blk_hdr			blkHdr;
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* --- extended or dse (dse is able to edit any header fields freely) --- */
	csd = cs_data;
	csa = cs_addrs;
	assert(REG2CSA(gv_cur_region) == csa);
	assert((csa->total_blks <= csa->ti->total_blks) || !IS_MCODE_RUNNING);
	assert(blk >= 0);
	/* Note: Even in snapshots, only INTEG requires mm_read to read FREE blocks. The assert below should be modified
	 * if we later introduce a scheme where we can figure out as to who started the snapshots and assert accordingly
	 */
	buff = (MM_BASE_ADDR(csa) + ((off_t)csa->hdr->blk_size * blk));
	read_only = gv_cur_region->read_only;
	INCR_GVSTATS_COUNTER(csa, csa->nl, n_dsk_read, 1);
	if (blk < csa->total_blks)	/* test against process private copy of total_blks */
	{	/* Only non-upgrade-involving version bumping is done here: when a V7 database was created with the first
		 * V7 version, we need to fix the version to account for adding intermediate version numbers. There is no
		 * juggling of actual block versions involved, and this operation is safe without any concucrency control.
		 */
		if ((GDSMV70000 == csd->creation_mdb_ver) && (GDSV6p == ((blk_hdr_ptr_t)buff)->bver))
		{
			if (read_only)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERDONLY, 3,
						DB_LEN_STR(gv_cur_region), 0, ERR_TEXT, 2,
						LEN_AND_LIT("read-only region needs to be read write and upgraded"));
			((blk_hdr_ptr_t)buff)->bver = GDSV7;
		}
		/* The second non-upgrade-related version bump has to do with initializing an empty bver. Bver is initialized
		 * for all bitmaps on an extend, and other free blocks are not read before being written to. 'dse add', however,
		 * does read the block it is modifying even if it is empty.
		 */
		if (GDSV4 == (expected = ((blk_hdr_ptr_t)buff)->bver)) /* WARNING - assignment */
		{
			newver = csd->desired_db_format;
			/* The read-only status of a statsdb region is meant to prevent substantive updates
			 * outside of the known logic. This is an update, but not a substantive one. And since the memory
			 * is not really read-only, we should permit it without issuing an error in this known case.
			 */
			if (read_only && !IS_STATSDB_REG(gv_cur_region))
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERDONLY, 3,
						DB_LEN_STR(gv_cur_region), 0, ERR_TEXT, 2,
						LEN_AND_LIT("read-only region needs to be read write"));
			ATOMIC_COMPARE_EXCHANGE_STRONG((&((blk_hdr_ptr_t)buff)->bver), &expected, newver,
					memory_order_relaxed, memory_order_relaxed);
		}
		return buff;
	}
	rdfail_detail = (blk < csa->ti->total_blks) ? cdb_sc_helpedout : cdb_sc_blknumerr;
	return (sm_uc_ptr_t)NULL;
}
