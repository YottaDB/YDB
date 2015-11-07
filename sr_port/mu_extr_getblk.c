/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "t_abort.h"
#include "gvcst_protos.h"	/* for gvcst_rtsib,gvcst_search,gvcst_search_blk prototype */
#ifdef VMS
#include <rms.h>		/* needed for muextr.h */
#endif
#include "muextr.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "min_max.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF sgmnt_addrs	*cs_addrs;
#ifdef UNIX
GBLREF unsigned int	t_tries;
#endif

error_def(ERR_GVGETFAIL);

int mu_extr_getblk(unsigned char *ptr, unsigned char *encrypted_buff_ptr)
{
	enum cdb_sc		status;
	int			bsiz;
	blk_hdr_ptr_t		bp;
	boolean_t		two_histories, end_of_tree;
	rec_hdr_ptr_t		rp;
	srch_blk_status		*bh;
	srch_hist		*rt_history;
#	ifdef UNIX
	DEBUG_ONLY(unsigned int	lcl_t_tries;)
	boolean_t		tn_aborted;
#	endif
#	ifdef GTM_CRYPT
	char			*in, *out;
	int			out_size, gtmcrypt_errno;
	trans_num		lcl_dirty;
	gd_segment		*seg;
	blk_hdr_ptr_t		encrypted_bp;
#	ifdef DEBUG
	static unsigned char	*private_blk;
	static int		private_blksz;
#	endif
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 != gv_target->root);
	t_begin(ERR_GVGETFAIL, 0);
	for (;;)
	{
		if (cdb_sc_normal != (status = gvcst_search(gv_currkey, NULL)))
		{
			t_retry(status);
			continue;
		}
		end_of_tree = two_histories = FALSE;
		bh = gv_target->hist.h;
		rp = (rec_hdr_ptr_t)(bh->buffaddr + bh->curr_rec.offset);
		bp = (blk_hdr_ptr_t)bh->buffaddr;
		if ((sm_uc_ptr_t)rp >= CST_TOB(bp))
		{
			rt_history = gv_target->alt_hist;
			if (cdb_sc_normal == (status = gvcst_rtsib(rt_history, 0)))
			{
				two_histories = TRUE;
				if (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, rt_history->h)))
				{
					t_retry(status);
					continue;
				}
				bp = (blk_hdr_ptr_t)rt_history->h[0].buffaddr;
			} else if (cdb_sc_endtree == status)
					end_of_tree = TRUE;
			else
			{
				t_retry(status);
				continue;
			}
		}
		assert(bp->bsiz <= cs_addrs->hdr->blk_size);
		bsiz = MIN(bp->bsiz, cs_addrs->hdr->blk_size);
		memcpy(ptr, bp, bsiz);
#		ifdef GTM_CRYPT
		if (NULL != encrypted_buff_ptr)
		{	/* The caller requested the encrypted buffer corresponding to `bp' as well. Take a copy of the encrypted
			 * twin global buffer. But, do that only if the cache record corresponding to `bp' is not dirty. Otherwise,
			 * the encrypted twin remains stale (as of dsk_read) until a wcs_wtstart happens at which point it updates
			 * the twin global buffer to contain the up-to-date encrypted contents. So, in this case, an explicit
			 * encryption is needed once this transaction succeeds.
			 */
			if (0 == (lcl_dirty = bh->cr->dirty))
			{
				encrypted_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, cs_addrs);
				memcpy(encrypted_buff_ptr, (sm_uc_ptr_t)encrypted_bp, bsiz);
			}
		}
#		endif
		UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
		if ((trans_num)0 != t_end(&gv_target->hist, two_histories ? rt_history : NULL, TN_NOT_SPECIFIED))
		{	/* Transaction succeeded. */
			if (two_histories)
				memcpy(gv_target->hist.h, rt_history->h, SIZEOF(srch_blk_status) * (rt_history->depth + 1));
#			ifdef GTM_CRYPT
			if (NULL != encrypted_buff_ptr)
			{
				assert(GTMCRYPT_INVALID_KEY_HANDLE != cs_addrs->encr_key_handle);
				if (lcl_dirty)
				{
					/* We did not take a copy of the encrypted twin global buffer. Do explicit encryption now */
					memcpy(encrypted_buff_ptr, ptr, SIZEOF(blk_hdr));	/* Copy the block header. */
					in = (char *)ptr + SIZEOF(blk_hdr);
					out = (char *)encrypted_buff_ptr + SIZEOF(blk_hdr);
					out_size = bsiz - SIZEOF(blk_hdr);
					GTMCRYPT_ENCRYPT(cs_addrs, cs_addrs->encr_key_handle, in, out_size, out, gtmcrypt_errno);
					if (0 != gtmcrypt_errno)
					{
						seg = cs_addrs->region->dyn.addr;
						GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
					}
				}
#				ifdef DEBUG
				else
				{	/* We took a copy of the encrypted twin global buffer before t_end. Ensure that we did not
					 * take a stale copy.
					 */
					if ((0 == private_blksz) || (private_blksz < cs_addrs->hdr->blk_size))
					{	/* [re]allocate space for doing out-of-place encryption. */
						if (NULL != private_blk)
							free(private_blk);
						private_blksz = cs_addrs->hdr->blk_size;
						private_blk = (unsigned char *)malloc(private_blksz);
					}
					memcpy(private_blk, ptr, SIZEOF(blk_hdr));
					in = (char *)ptr + SIZEOF(blk_hdr);
					out = (char *)private_blk + SIZEOF(blk_hdr);
					out_size = bsiz - SIZEOF(blk_hdr);
					GTMCRYPT_ENCRYPT(cs_addrs, cs_addrs->encr_key_handle, in, out_size, out, gtmcrypt_errno);
					assert(0 == gtmcrypt_errno);
					assert(0 == memcmp(private_blk, encrypted_buff_ptr, bsiz));
				}
#				endif
			}
#			endif
			return !end_of_tree;
		}
#		ifdef UNIX
		else
		{
			ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
			if (tn_aborted)
				return FALSE; /* global doesn't exist any more in the database */
		}
#		endif
	}
}
