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
#include "cmidef.h"
#include "cmmdef.h"

GBLREF connection_struct *curr_entry;
GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF gv_key		*gv_altkey;
GBLREF sgmnt_addrs	*cs_addrs;

bool gtcmtr_zprevious()
{
	bool		gvcst_zprevious();
	boolean_t	found;
	unsigned char	*ptr, regnum;
	unsigned short	top, old_top;
	unsigned int	len;
	gv_key		*save_key;
	cm_region_list	*reg_ref, *gtcm_find_region();

	ptr = curr_entry->clb_ptr->mbf;
	assert(CMMS_Q_PREV == *ptr);
	ptr++;
	len = (unsigned int)*((unsigned short *)ptr);
	ptr += sizeof(short);
	regnum = *ptr++;
	reg_ref = gtcm_find_region(curr_entry, regnum);
	len--;	/* subtract size of regnum */
	old_top = ((gv_key *)ptr)->top;
	top = gv_currkey->top;
	memcpy(gv_currkey, ptr, len);
	gv_currkey->top = top;
	gtcm_bind_name(reg_ref->reghead);
	if (gv_currkey->prev)
		found = (gv_target->root == 0) ? FALSE : gvcst_zprevious();
	else
	{	/* name level */
		assert(3 <= gv_currkey->end);			/* at least one character of name, and 2 <NUL> delimiters */
		assert((sizeof(mident) + 2) >= gv_currkey->end);	/* no more than an mident, and two <NUL> delimiters */
		assert(INVALID_GV_TARGET == reset_gv_target);
		for (;  ;)
		{
			reset_gv_target = gv_target;	/* for restoration, just in case something goes wrong before
							 * gtcm_bind_name() is called */
			gv_target = cs_addrs->dir_tree;
			found = gvcst_zprevious();
			if (!found)
				break;
			assert(3 <= gv_altkey->end);			/* at least one character of name and a <NUL> delimiter */
 			assert((sizeof(mident) + 2) >= gv_currkey->end);	/* no more than an mident & two <NUL> delimiters */
			save_key = gv_currkey;
			gv_currkey = gv_altkey;
			gv_altkey = save_key;
			gtcm_bind_name(reg_ref->reghead);
			reset_gv_target = INVALID_GV_TARGET;
			if ((NULL != gv_target->root) && (0 != gvcst_data()))
			{
				save_key = gv_currkey;
				gv_currkey = gv_altkey;
				gv_altkey = save_key;
				break;
			}
		}
	}
	if (!found)
		len = 0;
	else
	{
		gv_altkey->prev = gv_currkey->prev;
		if (gv_target->nct || gv_target->collseq)
			gv_xform_key(gv_altkey, TRUE);
		len = sizeof(gv_key) + gv_altkey->end;
	}
	ptr = curr_entry->clb_ptr->mbf;
	*ptr++ = CMMS_R_PREV;
	*((unsigned short *)ptr) = (unsigned short)(len + 1);
	ptr += sizeof(short);
	*ptr++ = regnum;
	if (len)
		memcpy(ptr, gv_altkey, len);
	((gv_key *)ptr)->top = old_top;
	curr_entry->clb_ptr->cbl = sizeof(unsigned char) + sizeof(unsigned short) + sizeof(unsigned char) + len;
	reset_gv_target = INVALID_GV_TARGET;
	return TRUE;
}
