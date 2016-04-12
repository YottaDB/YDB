/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "copy.h"

/* Include prototypes */
#include "t_qread.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk,gvcst_lftsib prototype */

/* WARNING:	assumes that the search history for the current target is in gv_target.hist */

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF unsigned char	rdfail_detail;
GBLREF srch_blk_status	*first_tp_srch_status;	/* overriding value of srch_blk_status given by t_qread in case of TP */
GBLREF unsigned int	t_tries;

enum cdb_sc	gvcst_lftsib(srch_hist *full_hist)
{
	srch_blk_status *old, *new, *old_base, *new_base;
	rec_hdr_ptr_t	rp;
	unsigned short	rec_size;
	enum cdb_sc	ret_val;
	block_id	blk;
	unsigned short	rtop, temp_short;
	sm_uc_ptr_t	buffer_address, bp;
	int4		cycle;

	new_base = &full_hist->h[0];
	old = old_base = &gv_target->hist.h[0];
	for (;;)
	{
		buffer_address = old->buffaddr;
		temp_short = old->prev_rec.offset;
		if (temp_short > 0)
			break;
		old++;
		if (0 == old->blk_num)
			return cdb_sc_endtree;
		if (old->cr && (old->blk_num != old->cr->blk))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_lostcr;
		}
		if (cdb_sc_normal != (ret_val = gvcst_search_blk(gv_currkey, old)))
			return ret_val;
	}
	/* old now points to the first block which had a non-zero prev_rec.offset */
	new = new_base + (old - old_base + 1);
	full_hist->depth = (uint4)(old - old_base);
	(new--)->blk_num = 0;
	new->blk_num = old->blk_num;
	new->tn = old->tn;
	new->cse = NULL;
	new->first_tp_srch_status = old->first_tp_srch_status;
	assert(new->level == old->level);
	assert(new->blk_target == old->blk_target);
	new->buffaddr = old->buffaddr;
	new->curr_rec = old->prev_rec;
	new->cycle = old->cycle;
	new->cr = old->cr;
	temp_short = new->curr_rec.offset;
	rp = (rec_hdr_ptr_t)(temp_short + new->buffaddr);
	GET_USHORT(rec_size, &rp->rsiz);
	rtop = temp_short + rec_size;
	if (((blk_hdr_ptr_t)new->buffaddr)->bsiz < rtop)
		return cdb_sc_rmisalign;
	bp = new->buffaddr;
	while (--new >= new_base)
	{
		--old;
		GET_LONG(blk, ((sm_int_ptr_t)(bp + rtop - SIZEOF(block_id))));
		new->tn = cs_addrs->ti->curr_tn;
		new->cse = NULL;
		if (NULL == (buffer_address = t_qread(blk, &new->cycle, &new->cr)))
			return((enum cdb_sc)rdfail_detail);
		new->first_tp_srch_status = first_tp_srch_status;
		assert(new->level == old->level);
		assert(new->blk_target == old->blk_target);
		new->blk_num = blk;
		new->buffaddr = buffer_address;
		bp = new->buffaddr;
		rtop = ((blk_hdr_ptr_t)new->buffaddr)->bsiz;
	}
	return cdb_sc_normal;
}
