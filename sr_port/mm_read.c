/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#define	MUPIP_UPGRADE_IN_PROGRESS	1	/* Copied from sr_port/mu_upgrade_bmm.c */

error_def(ERR_DBFILERDONLY);

sm_uc_ptr_t mm_read(block_id blk)
<<<<<<< HEAD
{	/* this is a kissing cousin to code in dsk_read and the two blocks should be maintained in parallel */
	boolean_t		fully_upgraded;
=======
{	/* this is a kissing cousin to code in dsk_read and the two blocs should be maintained in parallel */
	boolean_t		buff_is_modified_after_read = FALSE, fully_upgraded, read_only, was_crit;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	enum db_ver		tmp_ondskblkver;
	int			level;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		buff;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* --- extended or dse (dse is able to edit any header fields freely) --- */
<<<<<<< HEAD
	assert((cs_addrs->total_blks <= cs_addrs->ti->total_blks) || !IS_MCODE_RUNNING);
	assert(blk >= 0);
	assert(dba_mm == cs_addrs->hdr->acc_meth);
	csd = cs_data;
	fully_upgraded = csd->fully_upgraded;
	buff = (MM_BASE_ADDR(cs_addrs) + ((off_t)cs_addrs->hdr->blk_size * blk));
	if (blk < cs_addrs->total_blks)		/* test against process private copy of total_blks */
	{	/* seems OK - see if block needs to be converted to current version */
=======
	csd = cs_data;
	csa = cs_addrs;
	assert((csa->total_blks <= csa->ti->total_blks) || !IS_MCODE_RUNNING);
	assert(blk >= 0);
	tmp_ondskblkver = (enum db_ver)csd->desired_db_format;
	fully_upgraded = csd->fully_upgraded;
	buff = (MM_BASE_ADDR(csa) + ((off_t)csa->hdr->blk_size * blk));
	read_only = csd->read_only;
	INCR_GVSTATS_COUNTER(csa, csa->nl, n_dsk_read, 1);
	if (blk < csa->total_blks)		/* test against process private copy of total_blks */
	{	/* see if block needs to be converted to current version. This code block should be maintained in parallel
		 * with a similar section in mm_read */
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
		if ((GDSV6p == (tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver)) && (GDSMV70000 == csd->creation_mdb_ver))
		{       /* adjust for shift of GDSV7 id from 2 to 4 */
			tmp_ondskblkver = GDSV7;
			buff_is_modified_after_read = TRUE;
		}
		level = (int)((blk_hdr_ptr_t)buff)->levl;	/* Doing this here for the assert below */
		if (GDSV4 == tmp_ondskblkver)
<<<<<<< HEAD
		{	/* but might be uninitialed */
			/* V6 might not be correct, but any writer should correct it before it goes to a DB file */
			tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver = csd->desired_db_format;
		} else
			assert((GDSV7 == tmp_ondskblkver) || (GDSV6 == tmp_ondskblkver)	/* vanilla cases */
			|| ((GDSV7m == tmp_ondskblkver) && IS_64_BLK_ID(buff)) 	/* block upgrade complete from V6 */
			|| (!fully_upgraded && (GDSV6p == tmp_ondskblkver)));	/* shuffled & adjusted but still 4 byte ID */
		if (!fully_upgraded && (GDSV7m != tmp_ondskblkver))		/* !fully_upgraded only during V6 -> V7 upgrade */
		{	/* block in need of attention */
			if ((0 == (level = (int)((blk_hdr_ptr_t)buff)->levl)) || (LCL_MAP_LEVL == level)) /* WARNING assignment */
			{
				((blk_hdr_ptr_t)buff)->bver = GDSV7m;	/* bit map & data blocks just get version */
				DEBUG_ONLY(tmp_ondskblkver = GDSV7m);
			} else if ((csd->offset) && (GDSV6 == tmp_ondskblkver))
			{	/* This is a pre-V7 index block needing its offset adjusted */
				assert(MEMCMP_LIT(csd->label, GDS_LABEL));
				blk_ptr_adjust(buff, csd->offset);
				((blk_hdr_ptr_t)buff)->bver = GDSV6p;	/* 4 byte block_id with offset applied */
				DEBUG_ONLY(tmp_ondskblkver = GDSV6p);
			} else
				assert(GDSV6p == tmp_ondskblkver);
=======
		{	/* but might be uninitialized */
			tmp_ondskblkver = csd->desired_db_format;
			buff_is_modified_after_read = TRUE;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
		}
#ifdef	DEBUG_UPGRADE
		/* The following assert can trip in regular operation when dealing with recycled blocks after the DB becomes
		 * fully upgraded after an MUPIP UPGRADE and MUPIP REORG -UPGRADE complete. Because this assert was instrumental
		 * in debugging the various states of blocks during the upgrade process, we leave it in
		 */
		else
			assert((GDSV7 == tmp_ondskblkver) || (GDSV6 == tmp_ondskblkver)	/* vanilla cases */
				|| (LCL_MAP_LEVL == level)	/* This assert does not apply to local bit maps, ever */
				|| (0 == level)	/* This assert cannot apply to level 0 blocks. Except for directory tree level 0
						 * blocks, all level zero data blocks can be ANY version from V6 to V7m depending
						 * on when they were created. */
				|| ((GDSV7m == tmp_ondskblkver) && IS_64_BLK_ID(buff)) 	/* block upgrade complete from V6 */
				|| (!fully_upgraded && (GDSV6p == tmp_ondskblkver)));	/* shuffled & adjusted but still 4byte ID */
#endif
		if (LCL_MAP_LEVL == level)
		{	/* Local bit maps just get a version update because they were never counted in blk_to_upgrd */
			if ((GDSV7m > tmp_ondskblkver) && (GDSV7m == csd->desired_db_format))
			{	/* this in not necessary, but a nice touch */
				tmp_ondskblkver = GDSV7m;
				buff_is_modified_after_read = TRUE;
			}
		} else if ((csd->offset) && (GDSV6p > tmp_ondskblkver) && level)
		{	/* pre-V7 index block needing its offset adjusted */
			assert(MEMCMP_LIT(csd->label, GDS_LABEL) || (!fully_upgraded && (GDSV6p < csd->desired_db_format)));
			if (read_only)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(10) ERR_DBFILERDONLY, 3, DB_LEN_STR(gv_cur_region), 0,
					ERR_TEXT, 2, LEN_AND_LIT("read-only region needs to be read write and upgraded"));
			/* Deviation from the way dsk_read operates because dsk_read has the benefit of using
			 * its owned BG buffer. MM works directly on the file system copy. Grab crit to ensure
			 * that it sees a consistent copy of the block
			 * 	1) grab crit
			 * 	2) check the block header to ensure it needs an upgrade (version and level)
			 * 	3) call blk_ptr_adjust() as needed
			 */
			if (FALSE == (was_crit = csa->now_crit)) /* WARNING assigment */
				grab_crit(gv_cur_region, WS_15);
			if ((GDSV6p > ((blk_hdr_ptr_t)buff)->bver) && ((blk_hdr_ptr_t)buff)->levl &&
					(TRUE == blk_ptr_adjust(buff, csd->offset)))
			{	/* Do not mark the buffer as changed if a concurrency conflict occurred */
				((blk_hdr_ptr_t)buff)->bver = tmp_ondskblkver = GDSV6p;	/* 4 byte block_id offset applied */
				buff_is_modified_after_read = FALSE;	/* Redundant because this sets the bver */
				SHM_WRITE_MEMORY_BARRIER;
			}
			if (FALSE == was_crit)
				rel_crit(gv_cur_region);
		}
#ifdef	DEBUG_UPGRADE
		assert(!level || !fully_upgraded || (GDSV6p != tmp_ondskblkver) && (!MEMCMP_LIT(csd->label, GDS_LABEL)
			? ((GDSV7 == tmp_ondskblkver) || (GDSV7m == tmp_ondskblkver))
			: ((GDSV6 == tmp_ondskblkver) && !MEMCMP_LIT(csd->label, V6_GDS_LABEL))));
#endif
		assert(GDSV4 != tmp_ondskblkver);
		if (buff_is_modified_after_read)
		{
			if (read_only)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(10) ERR_DBFILERDONLY, 3, DB_LEN_STR(gv_cur_region), 0,
					ERR_TEXT, 2, LEN_AND_LIT("read-only region needs to be read write and upgraded"));
			/* MUPIP UPDATE: Increment block TN because block version changed */
			if ((MUPIP_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog) && !read_only)
				((blk_hdr_ptr_t)buff)->tn = (start_tn - ((1 < start_tn) ? 2 : 0));
			/* WARNING: modifying the block version without crit. Should be safe for such a small change
			 * but assert that the version is not going backwards */
			assert(((blk_hdr_ptr_t)buff)->bver <= tmp_ondskblkver);
			((blk_hdr_ptr_t)buff)->bver = tmp_ondskblkver;
			SHM_WRITE_MEMORY_BARRIER;
		}
		return buff;
	}
	rdfail_detail = (blk < csa->ti->total_blks) ? cdb_sc_helpedout : cdb_sc_blknumerr;
	return (sm_uc_ptr_t)NULL;
}
