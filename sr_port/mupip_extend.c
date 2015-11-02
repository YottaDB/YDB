/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#ifdef UNIX
#include "io.h"
#include "gtmsecshr.h"
#endif
#include "util.h"
#include "cli.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "mupip_exit.h"
#include "mupip_extend.h"
#include "gtmmsg.h"
#include "gdsfilext.h"
#include "wcs_backoff.h"
#if !defined(MM_FILE_EXT_OK)
#include "mu_rndwn_file.h"	/* for STANDALONE macro */
#endif

GBLREF gd_addr		*gd_header;
GBLREF gd_region 	*gv_cur_region;
GBLREF sgmnt_addrs 	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
#if !defined(MM_FILE_EXT_OK)
GBLREF boolean_t	have_standalone_access;
#endif

void mupip_extend(void)
{
	unsigned short	r_len;
	char		regionname[MAX_RN_LEN];
	uint4		bplmap, bit_maps, blocks, i, old_total, total;
	uint4		status;
	int		fd;
	int4		tblocks;

	error_def(ERR_DBOPNERR);
	error_def(ERR_MUNOACTION);
	error_def(ERR_MUNODBNAME);
	error_def(ERR_NOREGION);
	error_def(ERR_DBRDONLY);
	error_def(ERR_JNLFILOPN);
#ifdef VMS
	error_def(ERR_TEXT);
#endif

	r_len = SIZEOF(regionname);
	if (cli_get_str("REG_NAME", regionname, &r_len) == FALSE)
		rts_error(VARLSTCNT(1) ERR_MUNODBNAME);
	if (cli_get_int("BLOCKS",&tblocks))
	{
		if (tblocks < 1)
		{
			util_out_print("!/BLOCKS too small, no extension done",TRUE);
			mupip_exit(ERR_MUNOACTION);
		}
		blocks = tblocks;
	} else
		blocks = (uint4)-1;
	gvinit();
	for (i = 0, gv_cur_region = gd_header->regions; i < gd_header->n_regions; i++, gv_cur_region++)
	{
		if (memcmp(gv_cur_region->rname, regionname, r_len) == 0)
			break;
	}
	if (i >= gd_header->n_regions)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_NOREGION, 2, r_len, regionname);
		mupip_exit(ERR_MUNOACTION);
	}
	if ((dba_bg != gv_cur_region->dyn.addr->acc_meth) && (dba_mm != gv_cur_region->dyn.addr->acc_meth))
	{
		util_out_print("Can only EXTEND BG and MM databases",TRUE);
		mupip_exit(ERR_MUNOACTION);
	}
	if (reg_cmcheck(gv_cur_region))
	{
		util_out_print("!/Can't EXTEND region !AD across network",TRUE, REG_LEN_STR(gv_cur_region));
		mupip_exit(ERR_MUNOACTION);
	}
#if !defined(MM_FILE_EXT_OK)
	if (dba_mm == gv_cur_region->dyn.addr->acc_meth)
	{
		FILE_CNTL_INIT(gv_cur_region->dyn.addr);
		if (!STANDALONE(gv_cur_region))
		{
			util_out_print("Can't get standalone access to database file !AD with MM access method, no extension done.",
				TRUE, DB_LEN_STR(gv_cur_region));
			mupip_exit(ERR_MUNOACTION);
		}
		have_standalone_access = TRUE;
	}
#endif
	gvcst_init(gv_cur_region);				/* This should not happen as extend works on only */
	if (gv_cur_region->was_open)				/* one region at a time, but handle for safety */
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBOPNERR, 2, DB_LEN_STR(gv_cur_region));
		mupip_exit(ERR_MUNOACTION);
	}
	cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
	cs_data = cs_addrs->hdr;
	if ((uint4)-1 == blocks)
	{
		if (cs_addrs->hdr->extension_size == 0)
		{
			util_out_print("The extension size on file !AD is zero, no extension done.",TRUE,
				DB_LEN_STR(gv_cur_region));
			mupip_exit(ERR_MUNOACTION);
		}
		blocks = cs_addrs->hdr->extension_size;
	}
	/* cannot extend for read_only database. */
	if (gv_cur_region->read_only)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));
		mupip_exit(ERR_MUNOACTION);
	}
	switch(gv_cur_region->dyn.addr->acc_meth)
	{
		case dba_bg:
		case dba_mm:
			grab_crit(gv_cur_region);
			GRAB_UNFROZEN_CRIT(gv_cur_region, cs_addrs, cs_data);
			old_total = cs_addrs->ti->total_blks;
			if ((uint4)NO_FREE_SPACE == (status = gdsfilext(blocks, old_total)))
			{
				rel_crit(gv_cur_region);
				util_out_print("The extension failed on file !AD; check disk space and permissions.", TRUE,
					DB_LEN_STR(gv_cur_region));
				mupip_exit(ERR_MUNOACTION);
			} else
				assert(SS_NORMAL == status);
			total = cs_addrs->ti->total_blks;
			bplmap = cs_addrs->hdr->bplmap;
			bit_maps = DIVIDE_ROUND_UP(total, bplmap) - DIVIDE_ROUND_UP(old_total, bplmap);
			rel_crit(gv_cur_region);
			break;
		default:
			GTMASSERT;
	}
	util_out_print("Extension successful, file !AD extended by !UL blocks.  Total blocks = !UL.",TRUE,
		DB_LEN_STR(gv_cur_region), total - old_total - bit_maps, total - DIVIDE_ROUND_UP(total, bplmap));
	mupip_exit(SS_NORMAL);
}
