/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "iosp.h"
#include "copy.h"
#include "gdsbml.h"
#include "gtmio.h"
#include "mucblkini.h"
#include "anticipatory_freeze.h"
#include "jnl.h"

#define DIR_ROOT 1
#define DIR_DATA 2

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

void mucblkini (void)
{
	uchar_ptr_t		c, bmp;
	blk_hdr_ptr_t		bp1, bp2;
	rec_hdr_ptr_t		rp;
	unix_db_info		*udi;
	int4			tmp, bmpsize, status;

	udi = (unix_db_info *)gv_cur_region->dyn.addr->file_cntl->file_info;
	bp1 = (blk_hdr_ptr_t)malloc(cs_addrs->hdr->blk_size);
	bp2 = (blk_hdr_ptr_t)malloc(cs_addrs->hdr->blk_size);
	bmpsize = BM_SIZE(cs_addrs->hdr->bplmap);
	if (cs_addrs->do_fullblockwrites)
		bmpsize = (int4)ROUND_UP(bmpsize, cs_addrs->fullblockwrite_len);
	bmp = (uchar_ptr_t)malloc(bmpsize);
	LSEEKREAD(udi->fd, (off_t)(cs_addrs->hdr->start_vbn - 1) * DISK_BLOCK_SIZE, bmp, bmpsize, status);
	if (0 != status)
	{
		PERROR("Error reading first bitmap");
		return;
	}
	bml_busy(DIR_ROOT, bmp + SIZEOF(blk_hdr));
	bml_busy(DIR_DATA, bmp + SIZEOF(blk_hdr));
	DB_LSEEKWRITE(cs_addrs, udi->fn, udi->fd, (off_t)(cs_addrs->hdr->start_vbn - 1) * DISK_BLOCK_SIZE, bmp, bmpsize, status);
	if (0 != status)
	{
		PERROR("Error writing out first bitmap");
		return;
	}
	rp = (rec_hdr_ptr_t)((uchar_ptr_t)bp1 + SIZEOF(blk_hdr));
	BSTAR_REC(rp);
	c = CST_BOK(rp);
	tmp = DIR_DATA;
	PUT_LONG(c, tmp);
	bp1->bver = GDSVCURR;
	bp1->levl = 1;
	bp1->bsiz = BSTAR_REC_SIZE + SIZEOF(blk_hdr);
	bp1->tn = 0;
	bp2->bver = GDSVCURR;
	bp2->levl =0;
	bp2->bsiz = SIZEOF(blk_hdr);
	bp2->tn = 0;
	DSK_WRITE_NOCACHE(gv_cur_region, DIR_ROOT, (uchar_ptr_t)bp1, cs_addrs->hdr->desired_db_format, status);
	if (0 != status)
	{
		PERROR("Error writing to disk");
		exit(status);
	}
	DSK_WRITE_NOCACHE(gv_cur_region, DIR_DATA, (uchar_ptr_t)bp2, cs_addrs->hdr->desired_db_format, status);
	if (0 != status)
	{
		PERROR("Error writing to disk");
		exit(status);
	}
	return;
}
