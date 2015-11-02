/****************************************************************
 *								*
 *	Copyright 2004, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "copy.h"
#include "op.h"			/* for op_add prototype */

#define	GVINCR_PRE_INCR_MIN_BUFFLEN	MAX_NUM_SIZE	/* starting size of the malloced buffer to store pre_increment value */
							/* since the increment will mostly be a number, start with MAX_NUM_SIZE */

static	char		*gvincr_pre_incr_buff;		/* buffer to hold the pre-$INCR string value before converting to numeric */
static	int		gvincr_pre_incr_bufflen = 0;	/* length of the currently allocated buffer, updated if expansion occurs */
GBLREF	mval		*post_incr_mval;
GBLREF	gv_key		*gv_currkey;
GBLREF	mval		increment_delta_mval;	/* mval holding the increment value, set by op_gvincr */
GBLREF 	unsigned int	t_tries;

/* compute post_incr_mval from the current value of gv_currkey that was just now searched down the tree */
enum cdb_sc	gvincr_compute_post_incr(srch_blk_status *bh)
{
	int4		cur_blk_size;
	sm_uc_ptr_t	buffaddr;
	rec_hdr_ptr_t	rp;
	unsigned short	rec_size;
	int4		target_key_size, data_len;
	uint4		gvincr_malloc_len;
	mval		pre_incr_mval;
	int		tmp_cmpc;

	buffaddr = bh->buffaddr;
	cur_blk_size = ((blk_hdr_ptr_t)buffaddr)->bsiz;
	rp = (rec_hdr_ptr_t)(buffaddr + bh->curr_rec.offset);
	GET_USHORT(rec_size, &rp->rsiz);
	target_key_size = bh->curr_rec.match;
	assert(target_key_size == gv_currkey->end + 1);
	data_len = rec_size + EVAL_CMPC(rp) - SIZEOF(rec_hdr) - target_key_size;
	if ((0 > data_len) || (((sm_uc_ptr_t)rp + rec_size) > ((sm_uc_ptr_t)buffaddr + cur_blk_size)))
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_rmisalign;
	}
	if (data_len > gvincr_pre_incr_bufflen)
	{
		if (NULL != gvincr_pre_incr_buff)
			free(gvincr_pre_incr_buff);
		gvincr_malloc_len = (data_len > GVINCR_PRE_INCR_MIN_BUFFLEN) ? data_len
									: GVINCR_PRE_INCR_MIN_BUFFLEN;
		gvincr_pre_incr_buff = (char *)malloc(gvincr_malloc_len);
		gvincr_pre_incr_bufflen = gvincr_malloc_len;
	}
	/* malloced buffer is used for pre_incr_mval instead of stringpool because this is memory that is
	 * inherently used only by $INCREMENT and is needed only during the lifetime of the increment.
	 * keeping it in the stringpool causes it to stay until the next garbage collection which adds
	 * to unnecessary overheads.
	 */
	pre_incr_mval.mvtype = MV_STR;
	pre_incr_mval.str.addr = (char *)gvincr_pre_incr_buff;
	pre_incr_mval.str.len = data_len;
	memcpy(pre_incr_mval.str.addr, (sm_uc_ptr_t)rp + rec_size - data_len, data_len);
	op_add(&pre_incr_mval, &increment_delta_mval, post_incr_mval);
	assert(MV_IS_NUMERIC(post_incr_mval));
	/* "post_incr_mval" is of numeric type, convert it to a string type so it can be used by the caller to set "value" */
	MV_FORCE_STR(post_incr_mval);	/* will use stringpool to store string representation */
	/* "post_incr_mval" is a copy of the mval pointer passed to "op_gvincr" and hence is on the M-stack
	 * and therefore is known to the garbage collector (stp_gcol). hence it is ok for it to use the stringpool
	 */
	return cdb_sc_normal;
}
