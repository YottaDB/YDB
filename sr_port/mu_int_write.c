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
#include "dbfilop.h"
#include "mupint.h"

GBLREF unsigned char	*mu_int_locals;
GBLREF int4		mu_int_ovrhd;
GBLREF sgmnt_data	mu_int_data;
GBLREF gd_region	*gv_cur_region;

void mu_int_write(block_id blk, uchar_ptr_t ptr)
{
	file_control	*fc;

	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->op = FC_WRITE;
	fc->op_buff = ptr;
	fc->op_len = mu_int_data.blk_size;
	fc->op_pos = mu_int_ovrhd + (mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
	dbfilop(fc);
	return;
}
