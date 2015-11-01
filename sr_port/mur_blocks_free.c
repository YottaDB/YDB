/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "jnl.h"
#include "muprec.h"
#include "iosp.h"
#include "util.h"
#include "dbfilop.h"

GBLREF sgmnt_data_ptr_t	cs_data;

#define BPL	sizeof(int4)*8/BML_BITS_PER_BLK					/* blocks masked by a int4 */

int4 mur_blocks_free (ctl_list *ctl)
{
	int4		x;
	block_id 	bnum;
	int 		maps,mapsize,i,j,k,fcnt;
	unsigned char 	*disk, *c, *m_ptr;
	uint4 	*dskmap,map_blk_size;

	fcnt = 0;
	maps = (cs_data->trans_hist.total_blks + cs_data->bplmap - 1) / cs_data->bplmap;
	map_blk_size = BM_SIZE(cs_data->bplmap);
	m_ptr = (unsigned char*)malloc(cs_data->blk_size + 8);
	disk = (unsigned char *)(((int4)m_ptr) + 7 & -8);
	ctl->db_ctl->op_buff = (uchar_ptr_t)disk;
	ctl->db_ctl->op_len = cs_data->blk_size;
	for( i = 0; i != maps; i++ )
	{
		bnum = i * cs_data->bplmap;
		ctl->db_ctl->op = FC_READ;
		ctl->db_ctl->op_pos = cs_data->start_vbn + (cs_data->blk_size / 512 * bnum);
		dbfilop(ctl->db_ctl);
		if (((blk_hdr *) disk)->bsiz != map_blk_size)
		{
			util_out_print("Wrong size map block",TRUE); continue;
		}
		if (((blk_hdr *) disk)->levl != LCL_MAP_LEVL)
		{
			util_out_print("Local map block level incorrect.",TRUE);
		}
		mapsize = ( bnum == (cs_data->trans_hist.total_blks/cs_data->bplmap)*cs_data->bplmap ?
				     cs_data->trans_hist.total_blks - bnum : cs_data->bplmap );
		j = BPL;
		dskmap = (uint4*)(disk + sizeof(blk_hdr));
		while ( j < mapsize )
		{
			for ( k = 0; k!=sizeof(int4)*8; k += BML_BITS_PER_BLK )
			{
				fcnt += (*dskmap>>k) & 1;
			}
			j +=BPL;
			dskmap++;
		}
		c = (unsigned char *)dskmap;
		j -= BPL;
		j += 4;
		while (j < mapsize)
		{	for (k = 0; k != 8; k += BML_BITS_PER_BLK)
			{	fcnt += (*c >> k) & 1;
			}
			j += 4;
			c++;
		}

		x = (mapsize + 4 - j) * BML_BITS_PER_BLK;
		for ( k = 0; k < x; k += BML_BITS_PER_BLK )
		{	fcnt += (*c >> k) & 1;
		}
	}
	free(m_ptr);
	return fcnt;
}
