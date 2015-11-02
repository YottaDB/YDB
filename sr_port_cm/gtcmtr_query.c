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

#include <stddef.h>

#include "gtm_string.h"

#include "copy.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_find_region.h"
#include "gtcm_bind_name.h"
#include "gtcmtr_protos.h"
#include "gv_xform_key.h"
#include "gvcst_protos.h"	/* for gvcst_query,gvcst_queryget prototype */

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF gd_region        *gv_cur_region;

bool gtcmtr_query(void)
{
	unsigned char	*ptr, *gv_key_top_ptr, regnum;
	unsigned short	top, old_top;
	unsigned short	key_len, tmp_len, tot_val_len;
	cm_region_list	*reg_ref;
	mval		val;
	boolean_t	found, was_null;
	uint4		msg_len;

	ptr = curr_entry->clb_ptr->mbf;
	assert(CMMS_Q_QUERY == *ptr);
	ptr++;
	GET_USHORT(tmp_len, ptr);
	ptr += SIZEOF(short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	tmp_len--;	/* subtract size of regnum */
	assert(0 == offsetof(gv_key, top));
	GET_USHORT(old_top, ptr); /* old_top = ((gv_key *)ptr)->top; */
	CM_GET_GVCURRKEY(ptr, tmp_len);
	assert(1 == gv_currkey->base[gv_currkey->end - 2]);
	assert(0 != gv_currkey->prev || KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 3]);
	gtcm_bind_name(reg_ref->reghead, FALSE); /* gtcm_bind_name sets gv_target; do not use gv_target before gtcm_bind_name */
	if (gv_target->nct || gv_target->collseq)
	{ /* undo client appended 01 00 00 before user provided collation routine gets control */
		if (0 == gv_currkey->prev ||
			1 != gv_currkey->base[gv_currkey->prev] || 0 != gv_currkey->base[gv_currkey->prev + 1])
		{ /* name level $Q, or last subscript of incoming key not null */
			DEBUG_ONLY(was_null = FALSE;)
			gv_currkey->end -= 2;
			gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
		} else
		{ /* last subscript of incoming key null */
			DEBUG_ONLY(was_null = TRUE;)
			if (0 == gv_cur_region->std_null_coll)
				gv_currkey->base[gv_currkey->prev] = STR_SUB_PREFIX;
			else
			{
				gv_currkey->end -= 2;
				gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
			}
		}
		gv_xform_key(gv_currkey, FALSE);
		/* redo append 01 00 00 now that we are done with collation */
		if (0 == gv_currkey->prev || KEY_DELIMITER != gv_currkey->base[gv_currkey->end - 3] ||
				 gv_currkey->base[gv_currkey->end - 2] != (0 == gv_cur_region->std_null_coll ? STR_SUB_PREFIX :
				 	SUBSCRIPT_STDCOL_NULL))
		{
			assert(!was_null); /* null to non null transformation not allowed */
			gv_currkey->base[gv_currkey->end++] = 1;
			gv_currkey->base[gv_currkey->end++] = 0;
			gv_currkey->base[gv_currkey->end] = 0;
		} else
		{
			if (0 == gv_cur_region->std_null_coll)
				gv_currkey->base[gv_currkey->prev] = 1;
			else
			{
				gv_currkey->base[gv_currkey->end++]= 1;
				gv_currkey->base[gv_currkey->end++] = 0;
				gv_currkey->base[gv_currkey->end] = 0;
			}
		}
	}
	found = (0 != gv_target->root) ? (curr_entry->query_is_queryget ? gvcst_queryget(&val) : gvcst_query()) : FALSE;
	if (found)
	{
		if (gv_target->nct || gv_target->collseq)
			gv_xform_key(gv_altkey, TRUE);
		/* key_len = SIZEOF(gv_key) + gv_altkey->end; */
		key_len = gv_altkey->end + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(char);
		tot_val_len = (curr_entry->query_is_queryget ? val.str.len + SIZEOF(unsigned short) : 0);
		/* ushort <- uint4 assignment lossy? */
		assert((uint4)tot_val_len == (curr_entry->query_is_queryget ? val.str.len + SIZEOF(unsigned short) : 0));
	} else
		key_len = tot_val_len = 0;
	msg_len = SIZEOF(unsigned char) + SIZEOF(unsigned short) + SIZEOF(unsigned char) + key_len + tot_val_len;
	if (msg_len > curr_entry->clb_ptr->mbl)
		cmi_realloc_mbf(curr_entry->clb_ptr, msg_len);
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_QUERY;
	tmp_len = key_len + 1;
	PUT_USHORT(ptr, tmp_len);
	ptr += SIZEOF(unsigned short);
	*ptr++ = regnum;
	gv_key_top_ptr = ptr;
	if (found)
	{
		/* memcpy(ptr, gv_altkey, key_len); */ /* this memcpy modified to the following PUTs and memcpy; vinu, 07/18/01 */

		/* we are going to set ((gv_key *)ptr)->top to old_top, why even bother setting it to now to gv_altkey->top?
		 * vinu, 07/18/01 */

		/* PUT_USHORT(ptr, gv_altkey->top); */
		ptr += SIZEOF(unsigned short);
		PUT_USHORT(ptr, gv_altkey->end);
		ptr += SIZEOF(unsigned short);
		PUT_USHORT(ptr, gv_altkey->prev);
		ptr += SIZEOF(unsigned short);
		memcpy(ptr, gv_altkey->base, key_len - SIZEOF(unsigned short) - SIZEOF(unsigned short) - SIZEOF(unsigned short));
		ptr += (key_len - SIZEOF(unsigned short) - SIZEOF(unsigned short) - SIZEOF(unsigned short));
		if (curr_entry->query_is_queryget)
		{
			tmp_len = tot_val_len - SIZEOF(unsigned short);
			PUT_USHORT(ptr, tmp_len);
			ptr += SIZEOF(unsigned short);
			memcpy(ptr, val.str.addr, tmp_len);
		}
	}
	PUT_USHORT(gv_key_top_ptr, old_top); /* ((gv_key *)ptr)->top = old_top; */
	curr_entry->clb_ptr->cbl = msg_len;
	return TRUE;
}
