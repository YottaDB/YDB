/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF uint4			mu_upgrade_in_prog;

error_def(ERR_AUTODBCREFAIL);
error_def(ERR_FILECREERR);

/* This function is only called during the creation of a new region */
void mucblkini(enum db_ver desired_db_ver)
{
	uchar_ptr_t		c, bmp, bmp_base;
	blk_hdr_ptr_t		bp1, bp2;
	rec_hdr_ptr_t		rp;
	unix_db_info		*udi;
	int4			bmpsize, status;
	block_id		blk;
	boolean_t		isv7blk;

	isv7blk = (GDSV7m <= desired_db_ver);
	udi = FILE_INFO(gv_cur_region);
	bp1 = (blk_hdr_ptr_t)malloc(cs_addrs->hdr->blk_size);
	bp2 = (blk_hdr_ptr_t)malloc(cs_addrs->hdr->blk_size);
	bmpsize = BM_SIZE(cs_addrs->hdr->bplmap);
	if (cs_data->write_fullblk)
		bmpsize = (int4)ROUND_UP(bmpsize, cs_addrs->fullblockwrite_len);
	bmp_base = (uchar_ptr_t)malloc(bmpsize + OS_PAGE_SIZE);	/* UPGRADE opens an AIO DB, need to align buffer */
	bmp = (sm_uc_ptr_t)ROUND_UP2((sm_long_t)bmp_base, OS_PAGE_SIZE);
	DB_LSEEKREAD(udi, udi->fd, (off_t)BLK_ZERO_OFF(cs_addrs->hdr->start_vbn), bmp, bmpsize, status);
	if (0 != status)
	{
		PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_FILECREERR, 4, LEN_AND_LIT("reading first bitmap"),
				  DB_LEN_STR(gv_cur_region), status));
		free(bmp);
		free(bp1);
		free(bp2);
		return;
	}
	bml_busy(DIR_ROOT, bmp + SIZEOF(blk_hdr));
	bml_busy(DIR_DATA, bmp + SIZEOF(blk_hdr));
	ASSERT_NO_DIO_ALIGN_NEEDED(udi);	/* creating the database, so effectively have standalone access */
	DB_LSEEKWRITE(cs_addrs, udi, udi->fn, udi->fd, (off_t)BLK_ZERO_OFF(cs_addrs->hdr->start_vbn), bmp, bmpsize, status);
	free(bmp_base);
	if (0 != status)
	{
		free(bp1);
		free(bp2);
		PUTMSG_ERROR_CSA((CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_FILECREERR, 4, LEN_AND_LIT("writing out first bitmap"),
				  DB_LEN_STR(gv_cur_region), status));
		return;
	}
	rp = (rec_hdr_ptr_t)((uchar_ptr_t)bp1 + SIZEOF(blk_hdr));
	bstar_rec((sm_uc_ptr_t)rp, isv7blk); /* This function is creating a new region so it will always be making V7 blocks */
	c = CST_BOK(rp);
	blk = DIR_DATA;
	if (isv7blk)
		PUT_BLK_ID_64(c, blk);
	else
		PUT_BLK_ID_32(c, blk);
	bp1->bver = desired_db_ver;
	bp1->levl = 1;
	bp1->bsiz = bstar_rec_size(isv7blk) + SIZEOF(blk_hdr);
	bp1->tn = 0;
	bp2->bver = desired_db_ver;
	bp2->levl =0;
	bp2->bsiz = SIZEOF(blk_hdr);
	bp2->tn = 0;
	DSK_WRITE_NOCACHE(gv_cur_region, DIR_ROOT, (uchar_ptr_t)bp1, cs_addrs->hdr->desired_db_format, status);
	free(bp1);
	if (0 != status)
	{
		if (IS_MUMPS_IMAGE)
		{
			free(bp2);
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_AUTODBCREFAIL, 4, DB_LEN_STR(gv_cur_region),
				      REG_LEN_STR(gv_cur_region), status);
		} else
		{
			PERROR("Error writing to disk");
			free(bp2);
			EXIT(status);
		}
	}
	DSK_WRITE_NOCACHE(gv_cur_region, DIR_DATA, (uchar_ptr_t)bp2, cs_addrs->hdr->desired_db_format, status);
	free(bp2);
	if (0 != status)
	{
		if (IS_MUMPS_IMAGE)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_AUTODBCREFAIL, 4, DB_LEN_STR(gv_cur_region),
				REG_LEN_STR(gv_cur_region), status);
		else
		{
			PERROR("Error writing to disk");
			EXIT(status);
		}
	}
	return;
}
