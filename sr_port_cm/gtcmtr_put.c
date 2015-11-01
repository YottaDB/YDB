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
#include "gdsblk.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "cmmdef.h"
#include "gtcmtr_put.h"

#define LCL_BUF_SIZE 256

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_key		*gv_currkey;
GBLREF connection_struct *curr_entry;
GBLREF jnl_process_vector *prc_vec;

bool gtcmtr_put(void)
{
	cm_region_list	*gtcm_find_region(), *reg_ref;
	mval		v;
	char		buff[LCL_BUF_SIZE], *format_targ_key(), *end;
	unsigned char	*ptr, regnum;
	short		n;
	unsigned short	top, len;
	static readonly gds_file_id file;

	error_def(ERR_KEY2BIG);
	error_def(ERR_REC2BIG);
	error_def(ERR_GVIS);

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_Q_PUT);
	ptr++;
	len = *((unsigned short *)ptr);
	ptr += sizeof(unsigned short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry,regnum);
	len--; /* subtract size of regnum */
	top = gv_currkey->top;
	memcpy(gv_currkey, ptr, len);
	gv_currkey->top = top;
	ptr += len;
	gtcm_bind_name(reg_ref->reghead);

	if (JNL_ENABLED(cs_addrs->hdr))
	{
		prc_vec = curr_entry->pvec;
		assert(NULL != prc_vec);
		if ((0 == cs_addrs->jnl->pini_addr) || (0 == reg_ref->pini_addr))
		{
			grab_crit(gv_cur_region);
			jnl_ensure_open();
			if (!reg_ref->pini_addr)
			{
				cs_addrs->jnl->pini_addr = 0;
				jnl_put_jrt_pini(cs_addrs);
				reg_ref->pini_addr = cs_addrs->jnl->pini_addr;
			}
			rel_crit(gv_cur_region);
		}
		cs_addrs->jnl->pini_addr = reg_ref->pini_addr;
	}
	len = *((unsigned short *)ptr)++;
	v.mvtype = MV_STR;
	v.str.len = len;
	v.str.addr = (char *)ptr;
	if ((n = gv_currkey->end + 1) > gv_cur_region->max_key_size)
	{
		if ((end = format_targ_key(&buff[0], LCL_BUF_SIZE, gv_currkey, TRUE)) == 0)
			end = &buff[LCL_BUF_SIZE - 1];
		rts_error(VARLSTCNT(11) ERR_KEY2BIG, 4, n, (int4)gv_cur_region->max_key_size,
			REG_LEN_STR(gv_cur_region), 0, ERR_GVIS, 2, end - buff, buff);
	}
	if (n + v.str.len + sizeof(rec_hdr) > gv_cur_region->max_rec_size)
	{
		if ((end = format_targ_key(&buff[0], LCL_BUF_SIZE, gv_currkey, TRUE)) == 0)
			end = &buff[LCL_BUF_SIZE - 1];
		rts_error(VARLSTCNT(11) ERR_REC2BIG, 4, n + v.str.len + sizeof(rec_hdr),
			(int4)gv_cur_region->max_rec_size, REG_LEN_STR(gv_cur_region),
			0, ERR_GVIS, 2, end - buff, buff);
	}

	gvcst_put(&v);

	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_PUT;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
