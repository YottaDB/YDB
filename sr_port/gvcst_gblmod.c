/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "gdscc.h"		/* needed for tp.h */
#include "jnl.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"			/* needed for T_BEGIN_READ_NONTP_OR_TP macro */

#include "t_end.h"		/* prototypes */
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_protos.h"	/* for gvcst_gblmod,gvcst_search prototype */
#include "copy.h"
#include "gtmimagename.h" /* needed for spanning nodes */

GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;
LITREF mstr			nsb_dummy;

error_def(ERR_GBLMODFAIL);

bool	gvcst_gblmod(mval *v)
{
	boolean_t		gblmod, is_dummy;
	enum cdb_sc		status;
	int				key_size,  key_size2, data_len;
	srch_hist		*alt_history;
	blk_hdr_ptr_t	bp;
	rec_hdr_ptr_t	rp;
	unsigned short	match, match2, rsiz, offset_to_value, oldend;
	srch_blk_status	*bh;
	sm_uc_ptr_t		b_top;
	trans_num		tn_to_compare;

	T_BEGIN_READ_NONTP_OR_TP(ERR_GBLMODFAIL);
	assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
	for (;;)
	{
		gblmod = TRUE;
		if (cdb_sc_normal == (status = gvcst_search(gv_currkey, NULL)))
		{
			alt_history = gv_target->alt_hist;
			alt_history->h[0].blk_num = 0;

			VMS_ONLY(
				if (cs_addrs->hdr->resync_tn >= ((blk_hdr_ptr_t)gv_target->hist.h[0].buffaddr)->tn)
					gblmod = FALSE;
			)
#			ifdef UNIX
				tn_to_compare = ((blk_hdr_ptr_t)gv_target->hist.h[0].buffaddr)->tn;
				bh = gv_target->hist.h;
				bp = (blk_hdr_ptr_t) bh->buffaddr;
				rp = (rec_hdr_ptr_t) (bh->buffaddr + bh->curr_rec.offset);
				b_top = bh->buffaddr + bp->bsiz;
				GET_USHORT(rsiz, &rp->rsiz);
				key_size = gv_currkey->end + 1;
				data_len = rsiz + EVAL_CMPC(rp) - SIZEOF(rec_hdr) - key_size;
				match = bh->curr_rec.match;
				if (key_size == match)
				{
					if ((0 > data_len) || ((sm_uc_ptr_t)rp + rsiz > b_top))
					{
						status = cdb_sc_rmisalign1;
						t_retry(status);
						continue;
					}
					offset_to_value = SIZEOF(rec_hdr) + key_size - EVAL_CMPC(rp);
					/* If it could be a spanning node, i.e., has special value, then try to get tn from the
					 * block that contains the first special subscript. Since dummy nodes always have the
					 * same value, the tn number is not updated It s enough to do only the first piece
					 * since all pieces of a spanning node are killed  before an update is applied.
					 */
					if (IS_SN_DUMMY(data_len, (sm_uc_ptr_t)rp + offset_to_value))
					{
						oldend = gv_currkey->end;
						APPEND_HIDDEN_SUB(gv_currkey);
						if (cdb_sc_normal == (status = gvcst_search(gv_currkey, alt_history)))
						{
							key_size2 = gv_currkey->end + 1;
							match = alt_history->h[0].curr_rec.match;
							if (key_size2 == match)
								tn_to_compare =  ((blk_hdr_ptr_t)alt_history->h[0].buffaddr)->tn;
						}
						else
						{
							gv_currkey->end = oldend;
							gv_currkey->base[gv_currkey->end - 1] = KEY_DELIMITER;
							gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
							t_retry(status);
							continue;
						}
						gv_currkey->end = oldend;
						gv_currkey->base[gv_currkey->end - 1] = KEY_DELIMITER;
						gv_currkey->base[gv_currkey->end] = KEY_DELIMITER;
					}
				}
				if (cs_addrs->hdr->zqgblmod_tn > tn_to_compare)
					gblmod = FALSE;
#			endif
			if (!dollar_tlevel)
			{
				if ((trans_num)0 == t_end(&gv_target->hist, 0 == alt_history->h[0].blk_num ? NULL : alt_history,
						TN_NOT_SPECIFIED))
					continue;
			} else
			{
				status = tp_hist(0 == alt_history->h[0].blk_num ? NULL : alt_history);
				if (cdb_sc_normal != status)
				{
					t_retry(status);
					continue;
				}
			}
			return gblmod;
		}
		t_retry(status);
	}
}
