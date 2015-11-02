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
#include "filestruct.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcm_find_region.h"
#include "gtcm_bind_name.h"
#include "gvcst_protos.h"	/* for gvcst_order,gvcst_data prototype */
#include "gtcmtr_protos.h"
#include "gv_xform_key.h"

GBLREF connection_struct	*curr_entry;
GBLREF gv_namehead		*gv_target;
GBLREF gv_namehead		*reset_gv_target;
GBLREF gv_key			*gv_currkey;
GBLREF gv_key			*gv_altkey;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF gd_region		*gv_cur_region;

bool gtcmtr_order(void)
{
	boolean_t		found;
	unsigned char		*ptr, regnum;
	unsigned short		top, old_top, len, tmp_len;
	gv_key			*save_key;
	cm_region_list		*reg_ref;
	boolean_t		last_subsc_is_null;
	cm_region_head		*cm_reg_head;

	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);

	ptr = curr_entry->clb_ptr->mbf;
	assert(CMMS_Q_ORDER == *ptr);
	ptr++;
	GET_USHORT(len, ptr);
	ptr += SIZEOF(short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	len--;	/* subtract size of regnum */
	assert(0 == offsetof(gv_key, top));
	GET_USHORT(old_top, ptr); /* old_top = ((gv_key *)ptr)->top; */
	CM_GET_GVCURRKEY(ptr, len);
	assert(0 == gv_currkey->prev || 1 == gv_currkey->base[gv_currkey->end - 2]); /* name level $O, or 1 appended to key by
											op_gvorder on client */
	cm_reg_head = reg_ref->reghead;
	if (gv_currkey->prev)
	{
		gtcm_bind_name(cm_reg_head, FALSE); /* sets gv_target; do not use gv_target before gtcm_bind_name */
		if (gv_target->collseq || gv_target->nct)
		{	/* undo client's change before we transfer control to user collation routine
			 * so that user gets a clean key to transform */
			assert(3 <= gv_currkey->end);
			/* if region has standard null collation, assert that client op_gvorder should never have sent a key
			 * with a trailing sequence of bytes ... 00 01 00 00 (assuming gv_currkey->end points to the last 00).
			 */
			assert((0 == gv_cur_region->std_null_coll) || (KEY_DELIMITER != gv_currkey->base[gv_currkey->end - 3]));
			if (KEY_DELIMITER != gv_currkey->base[gv_currkey->end - 3])
			{
				last_subsc_is_null = FALSE;
				gv_currkey->base[gv_currkey->end - 2] = KEY_DELIMITER;
				gv_currkey->end--;
			} else
			{
				last_subsc_is_null = TRUE;
				gv_currkey->base[gv_currkey->end - 2] = STR_SUB_PREFIX;
				/* We have no way to tell if this is a fake NULL subscript that zwrite
				 * created (client side, see gvzwr_var), or a user supplied NULL subscript.
				 * We don't want to pass a fake NULL subscript to collation routines, but,
				 * since we can't tell if it IS fake, we pass it through to the collation
				 * routines and expect the routines to not transform NULL to non NULL.
				 */
			}
			gv_xform_key(gv_currkey, FALSE);
			assert(3 <= gv_currkey->end);
			/* Now do the same magic as in op_gvorder(), prepare the key for finding the "next" in order.
			 * op_gvorder relies on gv_curr_subsc_null and current region's std_null_coll setting.
			 * But since op_gvorder has sent this key from the client side, the magic has already been done.
			 * In this routine, we have already computed whether the last subscript is NULL or not. Use that below.
			 */
			/* assert that a null subscript is not transformed to a non-null by the collation routines */
			assert(!last_subsc_is_null ||
				((KEY_DELIMITER == gv_currkey->base[gv_currkey->end - 3])
					&& (STR_SUB_PREFIX == gv_currkey->base[gv_currkey->end - 2])));
			if (!last_subsc_is_null)
			{	/* last subscript is not null */
				gv_currkey->base[gv_currkey->end - 1] = 1;
				gv_currkey->base[gv_currkey->end + 1] = KEY_DELIMITER;
				gv_currkey->end++;
			} else
				gv_currkey->base[gv_currkey->prev] = 1;
		}
		found = (0 == gv_target->root) ? FALSE : gvcst_order();
		gv_altkey->prev = gv_currkey->prev;
	} else
	{	/* name level */
		assert(2 <= gv_currkey->end);			 /* at least one character of name, and two <NUL> delimiters */
		assert((MAX_MIDENT_LEN + 2) >= gv_currkey->end); /* no more than MAX_MIDENT_LEN (31), and two <NUL> delimiters */
		assert(INVALID_GV_TARGET == reset_gv_target);
		GTCM_CHANGE_REG(cm_reg_head);	/* sets gv_cur_region/cs_addrs/cs_data appropriately */
		for (;  ;)
		{
			reset_gv_target = gv_target;	/* for restoration, just in case something goes wrong before
							 * gtcm_bind_name() is called */
			gv_target = cs_addrs->dir_tree;
			found = gvcst_order();
			if (!found)
				break;
			assert(2 <= gv_altkey->end);			/* at least one character of name and a <NUL> delimiter */
			assert((MAX_MIDENT_LEN + 2) >= gv_currkey->end);/* no more than MAX_MIDENT_LEN (31),
									 * and two <NUL> delimiters */
			if ((PRE_V5_MAX_MIDENT_LEN < strlen((char *)gv_altkey->base)) && !curr_entry->client_supports_long_names)
			{
				rts_error(VARLSTCNT(6) ERR_UNIMPLOP, 0,
					ERR_TEXT, 2,
					LEN_AND_LIT("GT.CM client does not support global names longer than 8 characters"));
			}
			save_key = gv_currkey;
			gv_currkey = gv_altkey;
			gv_altkey = save_key;
			gtcm_bind_name(cm_reg_head, TRUE);
			reset_gv_target = INVALID_GV_TARGET;
			if ((0 != gv_target->root) && (0 != gvcst_data()))
			{
				save_key = gv_currkey;
				gv_currkey = gv_altkey;
				gv_altkey = save_key;
				break;
			}
			gv_currkey->base[gv_currkey->end - 1] = 1;
			gv_currkey->base[gv_currkey->end + 1] = 0;
			gv_currkey->end += 1;

		}
	}
	if (!found)
		len = 0;
	else
	{
		if (gv_target->nct || gv_target->collseq)
			gv_xform_key(gv_altkey, TRUE);
		/* len = SIZEOF(gv_key) + gv_altkey->end; */
		len = gv_altkey->end + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(unsigned short) +
		      SIZEOF(char);
	}
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_ORDER;
	tmp_len = len + 1;
	PUT_USHORT(ptr, tmp_len);
	ptr += SIZEOF(short);
	*ptr++ = regnum;
	if (len)
	{
		/* memcpy(ptr, gv_altkey, len); */ /* this memcpy modified to the following PUTs and memcpy; vinu, 07/18/01 */
		/* we are goint to restore top from old_top, why even bother setting it now? vinu, 07/18/01 */
		/* PUT_USHORT(ptr, gv_altkey->top); */
		PUT_USHORT(ptr + SIZEOF(unsigned short), gv_altkey->end);
		PUT_USHORT(ptr + SIZEOF(unsigned short) + SIZEOF(unsigned short), gv_altkey->prev);
		memcpy(ptr + SIZEOF(unsigned short) + SIZEOF(unsigned short) + SIZEOF(unsigned short), gv_altkey->base,
		       len - SIZEOF(unsigned short) - SIZEOF(unsigned short) - SIZEOF(unsigned short));
	}
	PUT_USHORT(ptr, old_top); /* ((gv_key *)ptr)->top = old_top; */
	curr_entry->clb_ptr->cbl = SIZEOF(unsigned char) + SIZEOF(unsigned short) + SIZEOF(unsigned char) + len;
	reset_gv_target = INVALID_GV_TARGET;
	return TRUE;
}
