/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

error_def(ERR_BADGTMNETMSG);
error_def(ERR_GVIS);
error_def(ERR_KEY2BIG);
error_def(ERR_REC2BIG);

cm_op_t gtcmtr_bufflush(void)
{
	cm_region_list	*reg_ref;
	mval		v;
	short		n;
	unsigned short	num_trans, data_len, key_end, key_prev;
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	unsigned char	*ptr, regnum, len, cc, prv;
	static readonly gds_file_id file;

	ASSERT_IS_LIBGNPSERVER;
	ptr = curr_entry->clb_ptr->mbf;
	assert(*ptr == CMMS_B_BUFFLUSH);
	ptr++;
	v.mvtype = MV_STR;
	CM_CHECK_AVAIL(curr_entry, ptr, SIZEOF(unsigned short));
	GET_USHORT(num_trans, ptr);
	ptr += SIZEOF(short);
	for (; num_trans-- > 0;)
	{
		/* "len", "cc" and "prv" are single bytes from the client. "cc" is the number of leading bytes this key
		 * shares with the previous one and "len" the number of bytes that follow it, so together they say where
		 * in "gv_currkey" to write and how much. Bounding them with an assert will not do, since a PRO build
		 * compiles it out, letting a client write up to 255 bytes at an offset of up to 255 into a key buffer
		 * that is DBKEYSIZE(reg->max_key_size) bytes and can be as small as ~64.
		 */
		CM_CHECK_AVAIL(curr_entry, ptr, 4 * SIZEOF(unsigned char));
		regnum = *ptr++;
		reg_ref = gtcm_find_region(curr_entry, regnum);
		len = *ptr++;
		cc = *ptr++;
		prv = *ptr++;
		CM_CHECK_AVAIL(curr_entry, ptr, len);
		if ((0 == len) || ((unsigned short)(len + cc) > gv_currkey->top))
			CM_BADMSG(curr_entry);
		memcpy(&gv_currkey->base[cc], ptr, len);
		ptr += len;
		/* Check the assembled key against the shape the runtime expects rather than asserting it. Check locals
		 * rather than the key itself: assigning first and rejecting afterwards leaves a client supplied "end"
		 * behind in gv_currkey, which the server goes on using, and the next region open then dies in
		 * gvcst_reservedDB_funcs() on "assert((SIZEOF(gv_key) + gv_currkey->end + 1) <= SIZEOF(save_currkey))".
		 * The bytes copied above stay inside the "(len + cc) > top" bound checked earlier, and the next
		 * accepted operation overwrites them.
		 */
		key_end = (unsigned short)(len + cc - 1);
		key_prev = (unsigned short)prv;
		if (CM_BAD_KEY_SHAPE(gv_currkey->base, key_end, key_prev, len + cc))
			CM_BADMSG(curr_entry);
		gv_currkey->end = key_end;	/* only now that the shape is known good */
		gv_currkey->prev = key_prev;
		if ((n = gv_currkey->end + 1) > gv_cur_region->max_key_size)
		{
			if ((end = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
				end = &buff[MAX_ZWR_KEY_SZ - 1];
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(11) ERR_KEY2BIG, 4, n, (int4)gv_cur_region->max_key_size,
				REG_LEN_STR(gv_cur_region), 0, ERR_GVIS, 2, end - buff, buff);
		}
		gtcm_bind_name(reg_ref->reghead, TRUE);
		if (JNL_ENABLED(cs_addrs->hdr))
		{
			cs_addrs->jnl->pini_addr = reg_ref->pini_addr;
			originator_prc_vec = curr_entry->pvec;
		}
		CM_CHECK_AVAIL(curr_entry, ptr, SIZEOF(unsigned short));
		GET_USHORT(data_len, ptr);
		ptr += SIZEOF(short);
		CM_CHECK_AVAIL(curr_entry, ptr, data_len);	/* gvcst_put() below reads "data_len" bytes from here */
		v.str.len = data_len;
		v.str.addr = (char *)ptr;
		if (n + v.str.len + SIZEOF(rec_hdr) > gv_cur_region->max_rec_size)
		{
			if ((end = format_targ_key(&buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
				end = &buff[MAX_ZWR_KEY_SZ - 1];
			RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(11) ERR_REC2BIG,
				4, n + v.str.len + SIZEOF(rec_hdr), (int4)gv_cur_region->max_rec_size,
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
	return CM_WRITE;
}
