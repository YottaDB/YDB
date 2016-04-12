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
#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "filestruct.h"
#include "jnl.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "copy.h"
#include "format_targ_key.h"
#include "gtcm_find_region.h"
#include "gtcm_bind_name.h"
#include "gvcst_protos.h"	/* for gvcst_put prototype */
#include "gtcmtr_protos.h"

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_key		*gv_currkey;
GBLREF connection_struct *curr_entry;
GBLREF jnl_process_vector *originator_prc_vec;

bool gtcmtr_bufflush(void)
{
	cm_region_list	*reg_ref;
	mval		v;
	short		n;
	unsigned short	num_trans, data_len;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	unsigned char	*ptr, regnum, len, cc, prv;
	static readonly gds_file_id file;

	error_def(ERR_KEY2BIG);
	error_def(ERR_REC2BIG);
	error_def(ERR_GVIS);

	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_B_BUFFLUSH);
	ptr++;
	v.mvtype = MV_STR;
	GET_USHORT(num_trans, ptr);
	ptr += SIZEOF(short);
	for (; num_trans-- > 0;)
	{
		regnum = *ptr++;
		reg_ref = gtcm_find_region(curr_entry, regnum);
		len = *ptr++;
		cc = *ptr++;
		prv = *ptr++;
		assert (len + cc - 1 < gv_currkey->top);
		memcpy(&gv_currkey->base[cc], ptr, len);
		ptr += len;
		gv_currkey->end = len + cc - 1;
		gv_currkey->prev = prv;
		assert(prv < gv_currkey->end);
		if ((n = gv_currkey->end + 1) > gv_cur_region->max_key_size)
		{
			if ((end = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
				end = &buff[MAX_ZWR_KEY_SZ - 1];
			rts_error(VARLSTCNT(11) ERR_KEY2BIG, 4, n, (int4)gv_cur_region->max_key_size,
				REG_LEN_STR(gv_cur_region), 0, ERR_GVIS, 2, end - buff, buff);
		}
		gtcm_bind_name(reg_ref->reghead, TRUE);
		if (JNL_ENABLED(cs_addrs->hdr))
		{
			cs_addrs->jnl->pini_addr = reg_ref->pini_addr;
			originator_prc_vec = curr_entry->pvec;
		}
		GET_USHORT(data_len, ptr);
		ptr += SIZEOF(short);
		v.str.len = data_len;
		v.str.addr = (char *)ptr;
		if (n + v.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)
		{
			if ((end = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
				end = &buff[MAX_ZWR_KEY_SZ - 1];
			rts_error(VARLSTCNT(11) ERR_REC2BIG, 4, n + v.str.len + SIZEOF(rec_hdr), (int4)gv_cur_region->max_rec_size,
				REG_LEN_STR(gv_cur_region), 0, ERR_GVIS, 2, end - buff, buff);
		}
		gvcst_put(&v);
		if (JNL_ENABLED(cs_addrs->hdr))
			reg_ref->pini_addr = cs_addrs->jnl->pini_addr; /* In case  journal switch occurred */
		ptr += data_len;
	}
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_C_BUFFLUSH;
	curr_entry->clb_ptr->cbl = S_HDRSIZE;
	return TRUE;
}
