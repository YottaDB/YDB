/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
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
#include "filestruct.h"
#include "mlkdef.h"
#include "mlk_ops.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "locklits.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "gtcml.h"
#include "mlk_pvtblk_equ.h"
#include "copy.h"
#include "gtcm_find_region.h"
#include "mmrhash.h"

GBLREF connection_struct *curr_entry;

error_def(ERR_BADGTMNETMSG);

void gtcml_lklist(void)
{
	cm_region_list *reg_ref;
	unsigned char *ptr, *walk, regnum, list_len, i, translev, subcnt;
	int sub_index;
	unsigned short top,len;
	mlk_pvtblk *new_entry;
	mlk_pvtblk *inlist1, *inlist2;
	bool new;

	ASSERT_IS_LIBGNPSERVER;
	ptr = curr_entry->clb_ptr->mbf;
	ptr++; /* hdr */
	ptr++; /* laflag */
	ptr++; /* transaction number */

	CM_CHECK_AVAIL(curr_entry, ptr, SIZEOF(unsigned char));
	list_len = *ptr++;
	for (i = 0; i < list_len; i++)
	{
		new = TRUE;
		/* "len" counts the region number, transaction level and subscript count bytes that follow it, plus the
		 * lock name reference itself. Check that it covers those three, and that they were received, BEFORE
		 * subtracting them: subtracting first would turn a client-supplied 0 into 65533, sizing both the
		 * MLK_PVTBLK_ALLOC() and the memcpy() below out of a message buffer that is only
		 * CM_MSG_BUF_SIZE + CM_BUFFER_OVERHEAD (532) bytes to begin with.
		 */
		CM_CHECK_AVAIL(curr_entry, ptr, SIZEOF(unsigned short) + 3 * SIZEOF(unsigned char));
		GET_USHORT(len, ptr);
		ptr += SIZEOF(unsigned short);
		if (3 > len)
			CM_BADMSG(curr_entry);
		regnum = *ptr++;
		translev = *ptr++;
		subcnt = *ptr++;
		len -= 3;	/* subtract sizes of regnum, translev and subcnt */
		CM_CHECK_AVAIL(curr_entry, ptr, len);
		/* "subcnt" and "len" both come from the client and have to agree with each other: the lock name
		 * reference is a run of length-prefixed subscripts, and MLK_PVTBLK_SUBHASH_GEN() below walks exactly
		 * "subcnt" of them off "value" using each length byte to find the next. A "subcnt" larger than the
		 * number of subscripts actually present walks off the end of the block allocated here.
		 */
		for (walk = ptr, sub_index = 0; sub_index < subcnt; sub_index++)
		{
			if ((walk >= (ptr + len)) || ((size_t)*walk + 1 > (size_t)((ptr + len) - walk)))
				CM_BADMSG(curr_entry);
			walk += *walk + 1;
		}
		if (walk != (ptr + len))
			CM_BADMSG(curr_entry);
		reg_ref = gtcm_find_region(curr_entry,regnum);
		MLK_PVTBLK_ALLOC(len, subcnt, 0, new_entry);
		MLK_PVTCTL_INIT(new_entry->pvtctl, reg_ref->reghead->reg);
		memcpy(&new_entry->value[0], ptr, len);
		ptr += len;
		reg_ref->oper = PENDING;
		new_entry->translev = translev;
		new_entry->subscript_cnt = subcnt;
		new_entry->level = 0;
		new_entry->nref_length = len;
		MLK_PVTBLK_SUBHASH_GEN(new_entry);
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
