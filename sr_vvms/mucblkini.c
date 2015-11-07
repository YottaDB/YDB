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

#include <ssdef.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "efn.h"
#include "gdsbml.h"
#include "mucblkini.h"
#include "iosb_disk.h"
#include "iosp.h"

#define DIR_ROOT 1
#define DIR_DATA 2

GBLREF gd_region *gv_cur_region;
GBLREF sgmnt_addrs *cs_addrs;

void mucblkini(void)
{
	unsigned char		*c, *bmp;
	blk_hdr			*bp1, *bp2;
	rec_hdr			*rp;
	uint4			status;

	/* get space for directory tree root and 1st directory tree data block */
	bp1 = malloc(cs_addrs->hdr->blk_size);
	bp2 = malloc(cs_addrs->hdr->blk_size);
	bmp = malloc(cs_addrs->hdr->blk_size);
	status = dsk_read(0, bmp, NULL, FALSE);
	if (SS$_NORMAL != status)
		rts_error(VARLSTCNT(1) status);
	bml_busy(DIR_ROOT, bmp + SIZEOF(blk_hdr));
	bml_busy(DIR_DATA, bmp + SIZEOF(blk_hdr));

	DSK_WRITE_NOCACHE(gv_cur_region, 0, bmp, cs_addrs->hdr->desired_db_format, status);
	if (SS$_NORMAL != status)
		sys$exit((int4)status);

	rp = (char*)bp1 + SIZEOF(blk_hdr);
	BSTAR_REC(rp);
	c = CST_BOK(rp);
	*(block_id*)c = (block_id)DIR_DATA;
	bp1->bver = GDSVCURR;
	bp1->levl = 1;
	bp1->bsiz = BSTAR_REC_SIZE + SIZEOF(blk_hdr);
	bp1->tn = 0;
	bp2->bver = GDSVCURR;
	bp2->levl = 0;
	bp2->bsiz = SIZEOF(blk_hdr);
	bp2->tn = 0;

	DSK_WRITE_NOCACHE(gv_cur_region, DIR_ROOT, bp1, cs_addrs->hdr->desired_db_format, status);
	if (SS$_NORMAL != status)
		sys$exit((int4)status);

	DSK_WRITE_NOCACHE(gv_cur_region, DIR_DATA, bp2, cs_addrs->hdr->desired_db_format, status);
	if (SS$_NORMAL != status)
		sys$exit((int4)status);
}
