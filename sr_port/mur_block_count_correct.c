/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <errno.h>

#include "gtm_unistd.h"
#include "gtm_signal.h"
#include "gtm_stat.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "iosp.h"
#include "gdsfilext.h"		/* for gdsfilext() prototype */
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gdskill.h"		/* needed for tp.h */
#include "gdscc.h"		/* needed for tp.h */
#include "tp.h"

GBLREF 	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t 	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF 	jnl_gbls_t		jgbl;

error_def(ERR_DBFILERR);
error_def(ERR_DBUNDACCMT);
error_def(ERR_TEXT);
error_def(ERR_WAITDSKSPACE);

uint4 mur_block_count_correct(reg_ctl_list *rctl)
{
	gtm_uint64_t		native_size, size;
	sgmnt_data_ptr_t 	mu_data;
	int4			mu_int_ovrhd;
	uint4			total_blks;
	uint4			status;
	uint4                   new_bit_maps, bplmap, new_blocks, tmpcnt;
	enum db_acc_method      acc_meth;

	MUR_CHANGE_REG(rctl);
	mu_data = cs_data;
	acc_meth = mu_data->acc_meth;
	switch (acc_meth)
	{
		case dba_bg:
		case dba_mm:
			mu_int_ovrhd = (int4)DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data) + mu_data->free_space, DISK_BLOCK_SIZE);
			break;
		default:
			assertpro(FALSE && acc_meth);
	}
	assert(mu_int_ovrhd == (mu_data->start_vbn - 1));
	size = mu_int_ovrhd + (off_t)(mu_data->blk_size / DISK_BLOCK_SIZE) * (mu_data->trans_hist.total_blks + 1);
	native_size = gds_file_size(gv_cur_region->dyn.addr->file_cntl);
	if (native_size && (size < native_size))
	{
		total_blks = (dba_mm == acc_meth) ? cs_addrs->total_blks : cs_addrs->ti->total_blks;
		if (JNL_ENABLED(cs_addrs))
			cs_addrs->jnl->pini_addr = 0; /* Stop simulation of GTM process journal record writing (if any active)*/
		/* If journaling, gdsfilext will need to write an inctn record. The timestamp of that journal record will
		 * need to be adjusted to the current system time to reflect that it is recovery itself writing that record
		 * instead of simulating GT.M activity. Since the variable jgbl.dont_reset_gbl_jrec_time is still set, gdsfilext
		 * will NOT modify jgbl.gbl_jrec_time. Temporarily reset it to allow for adjustments to gbl_jrec_time.
		 */
		assert(jgbl.dont_reset_gbl_jrec_time);
		jgbl.dont_reset_gbl_jrec_time = FALSE;
		/* Calculate the number of blocks to add based on the difference between the real file size and the file size
		 * computed from the header->total_blks.  Takes into account that gdsfilext() will automatically add new_bit_maps
		 * to the amount of blocks we request.
		 */
		bplmap = cs_data->bplmap;
		new_blocks = (native_size - size)/(mu_data->blk_size / DISK_BLOCK_SIZE);
		new_bit_maps = DIVIDE_ROUND_UP(total_blks + new_blocks, bplmap) - DIVIDE_ROUND_UP(total_blks, bplmap);
		tmpcnt = new_blocks - new_bit_maps;
		/* Call GDSFILEXT only if the no of blocks by which DB needs to be extended is not '0' since GDSFILEXT() treats
		 * extension by count 0 as unavailability of space(NO_FREE_SPACE error). And in the following case, tmpcnt could
		 * be '0' on AIX because in MM mode AIX increases the native_size to the nearest multiple of OS_PAGE_SIZE.
		 * And this increase could be less than GT.M block size.*/
		if (tmpcnt && SS_NORMAL != (status = GDSFILEXT(new_blocks - new_bit_maps, total_blks, TRANS_IN_PROG_FALSE)))
		{
			jgbl.dont_reset_gbl_jrec_time = TRUE;
			return (status);
		}
		jgbl.dont_reset_gbl_jrec_time = TRUE;
#		ifdef DEBUG
		/* Check that the filesize and blockcount in the fileheader match now after the extend */
		size = mu_int_ovrhd + (off_t)(mu_data->blk_size / DISK_BLOCK_SIZE) * (mu_data->trans_hist.total_blks + 1);
		native_size = gds_file_size(gv_cur_region->dyn.addr->file_cntl);
		ALIGN_DBFILE_SIZE_IF_NEEDED(size, native_size);
		assert(size == native_size);
#		endif
	}
	return SS_NORMAL;
}
