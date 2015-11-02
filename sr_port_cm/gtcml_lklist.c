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
#include "filestruct.h"
#include "mlkdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "locklits.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_pvtblk_equ.h"
#include "copy.h"

GBLREF connection_struct *curr_entry;

void gtcml_lklist(void)
{
	cm_region_list *reg_ref, *gtcm_find_region();
	unsigned char *ptr, regnum, laflag, list_len, i, translev, subcnt;
	unsigned short top,len;
	mlk_pvtblk *new_entry;
	mlk_pvtblk *inlist1, *inlist2;
	bool new;

	ptr = curr_entry->clb_ptr->mbf;
	ptr++; /* hdr */
	laflag = *ptr++;
	laflag &= ~INCREMENTAL;
	ptr++; /* transaction number */

	list_len = *ptr++;
	for (i = 0; i < list_len; i++)
	{
		new = TRUE;
		GET_USHORT(len, ptr);
		ptr += SIZEOF(unsigned short);
		regnum = *ptr++;
		reg_ref = gtcm_find_region(curr_entry,regnum);
		len--; /* subtract size of regnum */
		translev = *ptr++; len--;
		subcnt = *ptr++; len--;
		new_entry = (mlk_pvtblk *)malloc(SIZEOF(mlk_pvtblk) + len - 1);
		memset(new_entry, 0, SIZEOF(mlk_pvtblk));
		memcpy(&new_entry->value[0], ptr, len);
		ptr += len;
		reg_ref->oper = PENDING;
		new_entry->region = reg_ref->reghead->reg;
		new_entry->translev = translev;
		new_entry->subscript_cnt = subcnt;
		new_entry->level = 0;
		new_entry->subscript_cnt = subcnt;
		new_entry->total_length = len;
		new_entry->ctlptr = (mlk_ctldata *)FILE_INFO(new_entry->region)->s_addrs.lock_addrs[0];
		if (!reg_ref->lockdata)
		{
			reg_ref->lockdata = new_entry;
			new_entry->trans = TRUE;
		}
		else
		{
			inlist1 = inlist2 = reg_ref->lockdata;
			while (inlist1 && !mlk_pvtblk_equ(new_entry,inlist1))
			{
				inlist2 = inlist1;
				inlist1 = inlist1->next;
			}
			if (inlist1)
			{
				assert(!inlist1->trans);
				inlist1->old = TRUE;
				inlist1->trans = TRUE;
				inlist1->translev = new_entry->translev;
				inlist2->next = inlist1->next;
				if (inlist1 != reg_ref->lockdata)
				{
					inlist1->next = reg_ref->lockdata;
					reg_ref->lockdata = inlist1;
				}
				free(new_entry);
			}
			else
			{
				new_entry->trans = TRUE;
				new_entry->next = reg_ref->lockdata;
				reg_ref->lockdata = new_entry;
			}
		}
		if (new)
			reg_ref->lks_this_cmd++;
	}
}
