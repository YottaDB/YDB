/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "tp.h"

/* Include prototypes */
#include "t_qread.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk,gvcst_rtsib prototype */

/* construct a new array which is the path to the
   right sibling of the leaf of the old array
  note: the offset's will be correct, but NOT the 'match' members
*/

/* WARNING:	assumes that the search history for the current target is in gv_target.hist */

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF unsigned char	rdfail_detail;
GBLREF unsigned int	t_tries;
GBLREF srch_blk_status	*first_tp_srch_status;	/* overriding value of srch_blk_status given by t_qread in case of TP */

enum cdb_sc	gvcst_rtsib(srch_hist *full_hist, int level)
{
	srch_blk_status *old, *new, *old_base, *new_base;
	rec_hdr_ptr_t	rp;
	enum cdb_sc	ret_val;
	block_id	blk;
	unsigned short	rec_size, temp_short;
	sm_uc_ptr_t	buffer_address;
	int4		blk_id_sz, cycle;
	boolean_t	long_blk_id;

	new_base = &full_hist->h[level];
	/* The following assert should never trip; if it does, it means that this function will access an invalid part of
	 * the history. It is the responsibility of the caller to maintain this invariant.
	 */
	assert(level <= gv_target->hist.depth);
	old = old_base = &gv_target->hist.h[level];
	for (;;)
	{
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
		buffer_address = old->buffaddr;
		temp_short = old->curr_rec.offset;
		rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)buffer_address + temp_short);
		GET_USHORT(rec_size, &rp->rsiz);
		if ((sm_uc_ptr_t)rp + rec_size > buffer_address + (unsigned int)((blk_hdr_ptr_t)buffer_address)->bsiz)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
		if ((unsigned int)((blk_hdr_ptr_t)buffer_address)->bsiz > (temp_short + rec_size))
			break;
	}
	/* old now points to the first block which did not have a star key pointer*/
	new = new_base + (old - old_base + 1);
	full_hist->depth = (int)(level + old - old_base);
	(new--)->blk_num = 0;
	new->tn = old->tn;
	new->cse = NULL;
	new->first_tp_srch_status = old->first_tp_srch_status;
	assert(new->level == old->level);
	assert(new->blk_target == old->blk_target);
	new->blk_num = old->blk_num;
	new->buffaddr = old->buffaddr;
	new->prev_rec = old->curr_rec;
	new->cycle = old->cycle;
	new->cr = old->cr;
	temp_short = new->prev_rec.offset;
	rp = (rec_hdr_ptr_t)(temp_short + new->buffaddr);
	GET_USHORT(rec_size, &rp->rsiz);
	temp_short += rec_size;
	new->curr_rec.offset = temp_short;
	new->curr_rec.match = 0;
	if (((blk_hdr_ptr_t)old->buffaddr)->bsiz < temp_short)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_rmisalign;
	}
	rp = (rec_hdr_ptr_t)(old->buffaddr + temp_short);
	long_blk_id = IS_64_BLK_ID(old->buffaddr);
	while (--new >= new_base)
	{
		--old;
		GET_USHORT(rec_size, &rp->rsiz);
		if (((sm_uc_ptr_t)rp + rec_size) > (buffer_address + (unsigned int)((blk_hdr_ptr_t)buffer_address)->bsiz))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
		READ_BLK_ID(long_blk_id, &blk, (sm_uc_ptr_t)rp + rec_size - SIZEOF_BLK_ID(long_blk_id));
		new->tn = cs_addrs->ti->curr_tn;
		new->cse = NULL;
		if (NULL == (buffer_address = t_qread(blk, &new->cycle, &new->cr)))
			return((enum cdb_sc)rdfail_detail);
		long_blk_id = IS_64_BLK_ID(buffer_address);
		new->first_tp_srch_status = first_tp_srch_status;
		assert(new->level == old->level);
		assert(new->blk_target == old->blk_target);
		new->blk_num = blk;
		new->buffaddr = buffer_address;
		new->prev_rec.match = 0;
		new->prev_rec.offset = 0;
		new->curr_rec.match = 0;
		new->curr_rec.offset = SIZEOF(blk_hdr);
		rp = (rec_hdr_ptr_t)(buffer_address + SIZEOF(blk_hdr));
	}
	return cdb_sc_normal;
}
