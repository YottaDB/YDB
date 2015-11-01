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
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmtr_put.h"
#include "format_targ_key.h"
#define LCL_BUF_SIZE 256

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_key		*gv_currkey;
GBLREF connection_struct *curr_entry;
GBLREF jnl_process_vector *prc_vec;

bool gtcmtr_put(void)
{
	cm_region_list *reg_ref, *gtcm_find_region();
	unsigned char *ptr,regnum;
	unsigned short top,len;
	mval	v;
	error_def(ERR_KEY2BIG);
	error_def(ERR_REC2BIG);
	error_def(ERR_GVIS);
	char	buff[LCL_BUF_SIZE], *end;
	short	n;

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_PUT);
	ptr++;
	len = *((unsigned short *)ptr);
	ptr += sizeof(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	len--; /* subtract size of regnum */
	top = gv_currkey->top;
	memcpy(gv_currkey, ptr, len);
	gv_currkey->top = top;
	ptr += len;
	gtcm_bind_name(reg_ref->reghead);

	if (JNL_ENABLED(cs_addrs->hdr))
	{
		prc_vec = curr_entry->pvec;
		if (!reg_ref->pini_addr)
		{
			cs_addrs->jnl->pini_addr = 0;
			grab_crit(gv_cur_region);
			jnl_put_jrt_pini(cs_addrs);
			reg_ref->pini_addr = cs_addrs->jnl->pini_addr;
			rel_crit(gv_cur_region);
		}
		cs_addrs->jnl->pini_addr = reg_ref->pini_addr;
	}
	len = *((unsigned short *)ptr);
	ptr += sizeof(unsigned short);
	v.mvtype = MV_STR;
	v.str.len = len;
	v.str.addr = (char *)ptr;
	if ((n = gv_currkey->end + 1) > gv_cur_region->max_key_size)
	{	if ((end = format_targ_key(&buff[0], LCL_BUF_SIZE, gv_currkey, TRUE)) == 0)
		{	end = &buff[LCL_BUF_SIZE - 1];
		}
		rts_error(VARLSTCNT(10) ERR_KEY2BIG, 4, n, (int4)gv_cur_region->max_key_size,
			gv_cur_region->rname_len, &gv_cur_region->rname[0],
			ERR_GVIS, 2, end - &buff[0], &buff[0]);
	}
	if (n + v.str.len + sizeof(rec_hdr) > gv_cur_region->max_rec_size)
	{	if ((end = format_targ_key(&buff[0], LCL_BUF_SIZE, gv_currkey, TRUE)) == 0)
		{	end = &buff[LCL_BUF_SIZE - 1];
		}
		rts_error(VARLSTCNT(10) ERR_REC2BIG, 4, n + v.str.len + sizeof(rec_hdr),
			(int4)gv_cur_region->max_rec_size, gv_cur_region->rname_len, &gv_cur_region->rname[0],
			ERR_GVIS, 2, end - &buff[0], &buff[0]);
	}

	gvcst_put(&v);

	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_PUT;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
