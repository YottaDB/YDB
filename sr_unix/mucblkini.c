/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for EXIT() */
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

#define PUTMSG_ERROR_CSA(MSGPARMS)			\
MBSTART {						\
	if (IS_MUPIP_IMAGE)				\
		gtm_putmsg_csa MSGPARMS;		\
	else						\
		rts_error_csa MSGPARMS;			\
} MBEND

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;

error_def(ERR_AUTODBCREFAIL);
error_def(ERR_FILECREERR);

void mucblkini(void)
{
	uchar_ptr_t		c, bmp;
	blk_hdr_ptr_t		bp1, bp2;
	rec_hdr_ptr_t		rp;
	unix_db_info		*udi;
	int4			tmp, bmpsize, status;

	udi = FILE_INFO(gv_cur_region);
	bp1 = (blk_hdr_ptr_t)malloc(cs_addrs->hdr->blk_size);
	bp2 = (blk_hdr_ptr_t)malloc(cs_addrs->hdr->blk_size);
	bmpsize = BM_SIZE(cs_addrs->hdr->bplmap);
	if (cs_addrs->do_fullblockwrites)
		bmpsize = (int4)ROUND_UP(bmpsize, cs_addrs->fullblockwrite_len);
	bmp = (uchar_ptr_t)malloc(bmpsize);
	DB_LSEEKREAD(udi, udi->fd, (off_t)BLK_ZERO_OFF(cs_addrs->hdr->start_vbn), bmp, bmpsize, status);
	if (0 != status)
	{
		PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_FILECREERR, 4, LEN_AND_LIT("reading first bitmap"),
				  DB_LEN_STR(gv_cur_region), status));
		return;
	}
	bml_busy(DIR_ROOT, bmp + SIZEOF(blk_hdr));
	bml_busy(DIR_DATA, bmp + SIZEOF(blk_hdr));
	ASSERT_NO_DIO_ALIGN_NEEDED(udi);	/* because we are creating the database and so effectively have standalone access */
	DB_LSEEKWRITE(cs_addrs, udi, udi->fn, udi->fd, (off_t)BLK_ZERO_OFF(cs_addrs->hdr->start_vbn), bmp, bmpsize, status);
	free(bmp);
	if (0 != status)
	{
		PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_FILECREERR, 4, LEN_AND_LIT("writing out first bitmap"),
				  DB_LEN_STR(gv_cur_region), status));
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
	free(bp1);
	if (0 != status)
	{
		if (IS_MUMPS_IMAGE)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_AUTODBCREFAIL, 4, DB_LEN_STR(gv_cur_region),
				      REG_LEN_STR(gv_cur_region), status);
		else
		{
			PERROR("Error writing to disk");
			EXIT(status);
		}
	}
	DSK_WRITE_NOCACHE(gv_cur_region, DIR_DATA, (uchar_ptr_t)bp2, cs_addrs->hdr->desired_db_format, status);
	free(bp2);
	if (0 != status)
	{
		if (IS_MUMPS_IMAGE)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_AUTODBCREFAIL, 4, DB_LEN_STR(gv_cur_region),
				      REG_LEN_STR(gv_cur_region), status);
		else
		{
			PERROR("Error writing to disk");
			EXIT(status);
		}
	}
	return;
}
