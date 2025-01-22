/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
   mur_blocks_free = # of free blocks
*/
#include "mdef.h"

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "util.h"
#include "dbfilop.h"
#include "gds_blk_upgrade.h"	/* for gds_blk_upgrade prototype and GDS_BLK_UPGRADE_IF_NEEDED macro */

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;

#ifdef DEBUG
GBLREF	block_id	ydb_skip_bml_num;
#endif

#define BPL	SIZEOF(int4)*8/BML_BITS_PER_BLK					/* blocks masked by a int4 */

error_def(ERR_DBRDERR);
error_def(ERR_DYNUPGRDFAIL);

block_id mur_blocks_free(reg_ctl_list *rctl)
{
	block_id	bnum, maps, i, fcnt;
	enum db_ver	dummy_ondskblkver;
	file_control	*db_ctl;
	int		mapsize, j, k, status;
	int4		x;
	uint4		*dskmap, map_blk_size;
	unix_db_info	*udi;
	unsigned char	*c, *disk, *m_ptr;

	db_ctl = rctl->db_ctl;
	cs_data = rctl->csd;
	assert(FILE_INFO(gv_cur_region)->s_addrs.hdr == cs_data);
	fcnt = 0;
	maps = (cs_data->trans_hist.total_blks + cs_data->bplmap - 1) / cs_data->bplmap;
	map_blk_size = BM_SIZE(cs_data->bplmap);
	udi = FC2UDI(db_ctl);
	if (udi->fd_opened_with_o_direct)
	{	/* We need aligned buffers */
		m_ptr = (unsigned char *)malloc(ROUND_UP2(cs_data->blk_size, DIO_ALIGNSIZE(udi)) + OS_PAGE_SIZE);
		disk = (unsigned char *)ROUND_UP2((UINTPTR_T)m_ptr, OS_PAGE_SIZE);
	} else
	{
		m_ptr = (unsigned char *)malloc(cs_data->blk_size);
		assert(IS_PTR_8BYTE_ALIGNED(m_ptr));
		disk = m_ptr;
	}
	db_ctl->op_buff = (uchar_ptr_t)disk;
	db_ctl->op_len = cs_data->blk_size;
	for (i = 0; i != maps; i++)
<<<<<<< HEAD
	{	/* TODO: why mess with anything in bml other than version? */
#		ifdef DEBUG
		if ((0 != ydb_skip_bml_num) && (1 == i))
		{
			i = ydb_skip_bml_num / BLKS_PER_LMAP - 1;
			fcnt += (ydb_skip_bml_num - BLKS_PER_LMAP) / BLKS_PER_LMAP * (BLKS_PER_LMAP - 1);
			continue;
		}
#		endif
=======
	{
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
		bnum = i * cs_data->bplmap;
		db_ctl->op = FC_READ;
		db_ctl->op_pos = cs_data->start_vbn + ((gtm_int64_t)cs_data->blk_size / DISK_BLOCK_SIZE * bnum);
		status = dbfilop(db_ctl);
		if (SYSCALL_ERROR(status))
			RTS_ERROR_CSA_ABT(rctl->csa, VARLSTCNT(5) ERR_DBRDERR, 2, DB_LEN_STR(gv_cur_region), status);
		GDS_BLK_UPGRADE_IF_NEEDED(bnum, disk, disk, cs_data, &dummy_ondskblkver, status, cs_data->fully_upgraded);
		if (SS_NORMAL != status)
			if (ERR_DYNUPGRDFAIL == status)
				rts_error_csa(CSA_ARG(rctl->csa) VARLSTCNT(5) status, 3, bnum, DB_LEN_STR(gv_cur_region));
			else
				rts_error_csa(CSA_ARG(rctl->csa) VARLSTCNT(1) status);
		if (((blk_hdr *)disk)->bsiz != map_blk_size)
		{
			util_out_print("Wrong size map block", TRUE);
			continue;
		}
		if (((blk_hdr *)disk)->levl != LCL_MAP_LEVL)
			util_out_print("Local map block level incorrect.", TRUE);
		/* (cs_data->trans_hist.total_blks - bnum) can be cast because it should never be
		 * larger then BLKS_PER_LMAP when used
		 */
		assert((BLKS_PER_LMAP >= (cs_data->trans_hist.total_blks - bnum)) ||
				(bnum != ((cs_data->trans_hist.total_blks/cs_data->bplmap) * cs_data->bplmap)));
		mapsize = (bnum == ((cs_data->trans_hist.total_blks/cs_data->bplmap) * cs_data->bplmap) ?
					(int)(cs_data->trans_hist.total_blks - bnum) : cs_data->bplmap);
		j = BPL;
		dskmap = (uint4*)(disk + SIZEOF(blk_hdr));
		while (j < mapsize)
		{
			for (k = 0; k != SIZEOF(int4) * 8; k += BML_BITS_PER_BLK)
				fcnt += (*dskmap >> k) & 1;
			j += BPL;
			dskmap++;
		}
		c = (unsigned char *)dskmap;
		j -= BPL;
		j += 4;
		while (j < mapsize)
		{
			for (k = 0; k != 8; k += BML_BITS_PER_BLK)
				fcnt += (*c >> k) & 1;
			j += 4;
			c++;
		}
		x = (mapsize + 4 - j) * BML_BITS_PER_BLK;
		for (k = 0; k < x; k += BML_BITS_PER_BLK)
			fcnt += (*c >> k) & 1;
	}
	free(m_ptr);
	return fcnt;
}
