/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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

GBLREF	boolean_t		dse_running, mu_reorg_encrypt_in_prog, mu_reorg_upgrd_dwngrd_in_prog;
GBLREF	sgmnt_addrs 		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned char		rdfail_detail;

sm_uc_ptr_t mm_read(block_id blk)
{	/* this is a kissing cousin to code in dsk_read and the two blocks should be maintained in parallel */
	boolean_t		fully_upgraded;
	enum db_ver		tmp_ondskblkver;
	int			level;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		buff;

	/* --- extended or dse (dse is able to edit any header fields freely) --- */
	assert((cs_addrs->total_blks <= cs_addrs->ti->total_blks) || !IS_MCODE_RUNNING);
	assert(blk >= 0);
	assert(dba_mm == cs_addrs->hdr->acc_meth);
	csd = cs_data;
	tmp_ondskblkver = (enum db_ver)csd->desired_db_format;
	fully_upgraded = csd->fully_upgraded;
	buff = (MM_BASE_ADDR(cs_addrs) + ((off_t)cs_addrs->hdr->blk_size * blk));
	if (blk < cs_addrs->total_blks)		/* test against process private copy of total_blks */
	{	/* seems OK - see if block needs to be converted to current version */
		if ((GDSV6p == (tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver)) && (GDSMV70000 == csd->creation_mdb_ver))
		{       /* adjust for shift of GDSV7 id from 2 to 4 */
			tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver = GDSV7;
		}
		if (GDSV4 == tmp_ondskblkver)
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
				tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver = GDSV7m;	/* bit map & data blocks just get version */
			else if ((csd->offset) && (GDSV6 == tmp_ondskblkver))
			{	/* This is a pre-V7 index block needing its offset adjusted */
				assert(MEMCMP_LIT(csd->label, GDS_LABEL));
				blk_ptr_adjust(buff, csd->offset);
				tmp_ondskblkver = ((blk_hdr_ptr_t)buff)->bver = GDSV6p;	/* 4 byte block_id with offset applied */
			} else
				assert(GDSV6p == tmp_ondskblkver);
		}
		/* V7 block with V7 DB intent or V6 with V6 DB intent or V6 in transition to V7 */
		assert(!fully_upgraded ? (GDSV7 != tmp_ondskblkver)	/* note: GDSV6p cannot exist when fully_upgraded is TRUE */
			: (((GDSV7 == tmp_ondskblkver) || (GDSV7m == tmp_ondskblkver) && MEMCMP_LIT(csd->label, GDS_LABEL))
			|| ((GDSV6 == tmp_ondskblkver) && (!MEMCMP_LIT(csd->label, V6_GDS_LABEL)))));
		assert(GDSV4 != tmp_ondskblkver);
		SHM_WRITE_MEMORY_BARRIER;
		return buff;
	}
	rdfail_detail = (blk < cs_addrs->ti->total_blks) ? cdb_sc_helpedout : cdb_sc_blknumerr;
	return (sm_uc_ptr_t)NULL;
}
