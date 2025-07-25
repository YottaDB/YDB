/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_string.h"

#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsbml.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "stp_parms.h"
#include "iosp.h"
#include "jnl.h"
#include "io.h"
#include "gtmsecshr.h"
#include "util.h"
#include "cli.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "mupip_exit.h"
#include "mupip_extend.h"
#include "gtmmsg.h"
#include "gdsfilext.h"
#include "wcs_backoff.h"
#include "gt_timer.h"
#if defined(MM_FILE_EXT_OK)
#	define DB_IPCS_RESET(REG)
#else
#	 include "mu_rndwn_file.h"
#	 include "db_ipcs_reset.h"
#	define DB_IPCS_RESET(REG)			\
	{						\
		if (dba_mm == REG->dyn.addr->acc_meth)	\
			db_ipcs_reset(REG);		\
	}
#endif

GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	boolean_t		jnlpool_init_needed;

error_def(ERR_DBOPNERR);
error_def(ERR_DBRDONLY);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNODBNAME);
error_def(ERR_NOREGION);
error_def(ERR_TEXT);

void mupip_extend(void)
{
	unsigned short	r_len;
	char		regionname[MAX_RN_LEN + 1];
	uint4		bplmap, i;
	int4		status;
	block_id	blocks, bit_maps, tblocks, total, old_total, temp1, temp2;
	int		fd;
	boolean_t	defer_alloc;

	r_len = MAX_RN_LEN;
	jnlpool_init_needed = TRUE;
	if (cli_get_str("REG_NAME", regionname, &r_len) == FALSE)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_MUNODBNAME);
	assert((MAX_RN_LEN <= r_len) || ('\0' == regionname[r_len]));
	/* The following is only needed for the 'MAX_RN_LEN == r_len' case due to the above assert,
	 * but we do it all the time for simplicity.
	 */
	regionname[r_len] = '\0';
	cli_strupper(regionname);
	gvinit();
	for (i = 0, gv_cur_region = gd_header->regions; i < gd_header->n_regions; i++, gv_cur_region++)
	{
		if (memcmp(gv_cur_region->rname, regionname, r_len) == 0)
			break;
	}
	if (i >= gd_header->n_regions)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, r_len, regionname);
		mupip_exit(ERR_MUNOACTION);
	}
	if (!IS_REG_BG_OR_MM(gv_cur_region))
	{
		util_out_print("Can only EXTEND BG and MM databases",TRUE);
		mupip_exit(ERR_MUNOACTION);
	}
	if (reg_cmcheck(gv_cur_region))
	{
		util_out_print("!/Can't EXTEND region !AD across network",TRUE, REG_LEN_STR(gv_cur_region));
		mupip_exit(ERR_MUNOACTION);
	}
#	if !defined(MM_FILE_EXT_OK)
	if (dba_mm == gv_cur_region->dyn.addr->acc_meth)
	{
		FILE_CNTL_INIT(gv_cur_region->dyn.addr);
		if (!STANDALONE(gv_cur_region))
		{
			util_out_print("Can't get standalone access to database file !AD with MM access method, no extension done.",
				TRUE, DB_LEN_STR(gv_cur_region));
			mupip_exit(ERR_MUNOACTION);
		}
		assert((FILE_INFO(gv_cur_region))->grabbed_access_sem); /* we should have standalone access */
	}
#	endif
	gvcst_init(gv_cur_region);
	if (gv_cur_region->was_open)
	{	/* This should not happen as extend works on only one region at a time, but handle for safety */
		gtm_putmsg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(4) ERR_DBOPNERR, 2, DB_LEN_STR(gv_cur_region));
		DB_IPCS_RESET(gv_cur_region);
		mupip_exit(ERR_MUNOACTION);
	}
	cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
	cs_data = cs_addrs->hdr;
	defer_alloc = cs_data->defer_allocate;
	if (cli_get_int64("BLOCKS",&tblocks))
	{	/* tblocks can be 0 if defer_alloc is FALSE because the goal is to fully allocate an existing database */
		if ((tblocks < 0) || (defer_alloc && (tblocks == 0)))
		{
			util_out_print("!/BLOCKS too small, no extension done",TRUE);
			DB_IPCS_RESET(gv_cur_region);
			mupip_exit(ERR_MUNOACTION);
		}
		blocks = tblocks;
	} else
	{
		blocks = (uint4)-1;
		if (cs_addrs->hdr->extension_size == 0)
		{
			util_out_print("The extension size on file !AD is zero, no extension done.",TRUE,
				DB_LEN_STR(gv_cur_region));
			DB_IPCS_RESET(gv_cur_region);
			mupip_exit(ERR_MUNOACTION);
		}
		blocks = cs_addrs->hdr->extension_size;
	}
	/* cannot extend for read_only database. */
	if (gv_cur_region->read_only)
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		DB_IPCS_RESET(gv_cur_region);
		mupip_exit(ERR_MUNOACTION);
	}
	assertpro(IS_REG_BG_OR_MM(gv_cur_region));
	grab_crit(gv_cur_region, WS_67);
	GRAB_UNFROZEN_CRIT(gv_cur_region, cs_addrs, WS_68);
	old_total = cs_addrs->ti->total_blks;
	if (NO_FREE_SPACE == (status = GDSFILEXT(blocks, old_total, TRANS_IN_PROG_FALSE)))
	{
		rel_crit(gv_cur_region);
		util_out_print("The extension failed on file !AD; check disk space and permissions.", TRUE,
			DB_LEN_STR(gv_cur_region));
		DB_IPCS_RESET(gv_cur_region);
		mupip_exit(ERR_MUNOACTION);
	} else
		assert(SS_NORMAL == status);
	total = cs_addrs->ti->total_blks;
	bplmap = cs_addrs->hdr->bplmap;
	bit_maps = DIVIDE_ROUND_UP(total, bplmap) - DIVIDE_ROUND_UP(old_total, bplmap);
	rel_crit(gv_cur_region);
	temp1 = total - old_total - bit_maps;
	temp2 = total - DIVIDE_ROUND_UP(total, bplmap);
	util_out_print("Extension successful, file !AD extended by !@UQ blocks.  Total blocks = !@UQ.",TRUE,
		DB_LEN_STR(gv_cur_region), &temp1, &temp2);
	DB_IPCS_RESET(gv_cur_region); /* final cleanup (for successful case) before exit */
	mupip_exit(SS_NORMAL);
}
