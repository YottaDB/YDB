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

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsbml.h"
#include "dbfilop.h"
#include "mupint.h"

GBLREF sgmnt_data	mu_int_data;
GBLREF int4		mu_int_ovrhd;
GBLREF unsigned char	*mu_int_locals;
GBLREF gd_region	*gv_cur_region;

uchar_ptr_t mu_int_read(block_id blk)
{
	file_control	*fc;

	if (!bml_busy(blk,mu_int_locals))
		return 0;
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->op = FC_READ;
	fc->op_buff = (uchar_ptr_t)malloc(mu_int_data.blk_size);
	fc->op_len = mu_int_data.blk_size;
	fc->op_pos = mu_int_ovrhd + (mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
	dbfilop(fc);
	return fc->op_buff;
}
