/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_int4.h"
#include "hashtab_int8.h"
#include "hashtab_mname.h"
#include "buddy_list.h"
#include "muprec.h"
#include "gdsbml.h"
#include "iosp.h"
#include "gds_blk_upgrade.h"	/* for gds_blk_upgrade prototype and GDS_BLK_UPGRADE_IF_NEEDED macro */

/* Include prototypes */
#include "bit_set.h"
#include "bit_clear.h"
#include "dbfilop.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	reg_ctl_list		*mur_ctl;
GBLREF	int			mur_regno;

void	mur_master_map()
{
	uchar_ptr_t		bml_buffer;
	uint4			bplmap, blk_index, bml_size;
	bool			dummy;
	int			status;
	file_control		*db_ctl;
	enum db_ver		dummy_ondskblkver;

	error_def(ERR_DBRDERR);
	error_def(ERR_DYNUPGRDFAIL);

	assert(gv_cur_region == mur_ctl[mur_regno].gd);
	assert(cs_addrs == mur_ctl[mur_regno].csa);
	assert(cs_data == cs_addrs->hdr);
	db_ctl = mur_ctl[mur_regno].db_ctl;
	bplmap = cs_data->bplmap;
	bml_size = ROUND_UP(BML_BITS_PER_BLK * bplmap + sizeof(blk_hdr), 8);
	bml_buffer = (uchar_ptr_t)malloc(bml_size);
	for (blk_index = 0;  blk_index < cs_data->trans_hist.total_blks;  blk_index += bplmap)
	{	/* Read local bit map into buffer */
		db_ctl->op = FC_READ;
		db_ctl->op_buff = bml_buffer;
		db_ctl->op_len = bml_size;
		db_ctl->op_pos = cs_data->start_vbn + cs_data->blk_size / DISK_BLOCK_SIZE * blk_index;
		dbfilop(db_ctl);	/* No return if error */
		GDS_BLK_UPGRADE_IF_NEEDED(blk_index, db_ctl->op_buff, db_ctl->op_buff, cs_data, &dummy_ondskblkver, status);
		if (SS_NORMAL != status)
			if (ERR_DYNUPGRDFAIL == status)
				rts_error(VARLSTCNT(5) status, 3, blk_index, DB_LEN_STR(gv_cur_region));
			else
				rts_error(VARLSTCNT(1) status);
		if (bml_find_free(0, bml_buffer + sizeof(blk_hdr), bplmap, &dummy) == NO_FREE_SPACE)
			bit_clear(blk_index / bplmap, cs_addrs->bmm);
		else
			bit_set(blk_index / bplmap, cs_addrs->bmm);
	}

	/* Last local map may be smaller than bplmap so redo with correct bit count */
	if (bml_find_free(0, bml_buffer + sizeof(blk_hdr), cs_addrs->ti->total_blks - cs_addrs->ti->total_blks / bplmap * bplmap,
			  &dummy)
	    == NO_FREE_SPACE)
		bit_clear(blk_index / bplmap - 1, cs_addrs->bmm);
	else
		bit_set(blk_index / bplmap - 1, cs_addrs->bmm);

	free(bml_buffer);
	cs_addrs->nl->highest_lbm_blk_changed = cs_data->trans_hist.total_blks;
}
