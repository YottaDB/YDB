/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <varargs.h>

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"

/* Include prototypes */
#include "t_qread.h"
#include "min_max.h"
#include "hashtab_mname.h"
#include "hashtab.h"

static	mval		val;
static	char		buff[MAX_KEY_SZ];

GBLREF gd_addr		*gd_header;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;

gv_namehead *tp_get_target(sm_uc_ptr_t buffaddr)
/* given a pointer to a block, this routine returns a pointer to the gv_namehead of the global;
 * if the block contains only a star-key, it tries to work toward the leaf;
 * if the block is in the GVT, it currently gives a "wrong" answer;
 * if it encounters an error, it returns a NULL;
 */
{
	unsigned char	*cp;
	unsigned short	rsize;
	int		dummycycle, levl;
	sm_uc_ptr_t	rp, rtop;
	cache_rec_ptr_t	dummycr;
	gv_namehead	*cur_gv_target, *temp_gv_target;
	ht_ent_mname	*tabent;
	mname_entry	 gvent;

	for (levl = MAX_BT_DEPTH;;  levl--)
	{
		if ((NULL == buffaddr) || (IS_BML(buffaddr)) || (cs_data->blk_size < (int4)((blk_hdr_ptr_t)buffaddr)->bsiz)
				|| (0 > levl))
			return (gv_namehead *)NULL;
		rp = buffaddr + sizeof(blk_hdr);
		GET_USHORT(rsize, &((rec_hdr_ptr_t)rp)->rsiz);
		if (((blk_hdr_ptr_t)buffaddr)->bsiz < rsize + sizeof(blk_hdr))
			return (gv_namehead *)NULL;
		rtop = rp + rsize;
		rp += sizeof(rec_hdr);
		if ((BSTAR_REC_SIZE != rsize) || (0 == ((blk_hdr_ptr_t)buffaddr)->levl))
			break;
		buffaddr = t_qread(*(block_id_ptr_t)rp, (sm_int_ptr_t)&dummycycle, &dummycr);
	}
	if (NULL == val.str.addr)
		val.str.addr = (char *)buff;
	for (cp = (unsigned char *)val.str.addr; rp <= rtop; cp++)
		if ('\0' == (*cp = *rp++))
			break;
	if ('\0' != *cp)
		return (gv_namehead *)NULL;
	val.str.len = cp - (unsigned char *)val.str.addr;
	if (gv_target != cs_addrs->dir_tree)
	{
		/* this code is picked up from gv_bind_name */
		gvent.var_name.addr = val.str.addr;
		gvent.var_name.len = MIN(val.str.len, MAX_MIDENT_LEN);
		COMPUTE_HASH_MNAME(&gvent);
		if (NULL ==  (tabent = lookup_hashtab_mname(gd_header->tab_ptr, &gvent)))
			return (gv_namehead *)NULL;
		temp_gv_target = (gv_namehead *)tabent->value;
	}
	else
		temp_gv_target = gv_target;
	return temp_gv_target;
}
