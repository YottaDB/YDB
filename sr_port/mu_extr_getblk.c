/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtmcrypt.h"
#include "min_max.h"

GBLREF gd_region	*gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF unsigned int	t_tries;

error_def(ERR_GVGETFAIL);

int mu_extr_getblk(unsigned char *ptr, unsigned char *encr_ptr, boolean_t use_null_iv, int *got_encrypted_block)
{
	enum cdb_sc		status;
	int			bsiz;
	blk_hdr_ptr_t		bp;
	boolean_t		two_histories, end_of_tree;
	rec_hdr_ptr_t		rp;
	srch_blk_status		*bh;
	srch_hist		*rt_history;
	boolean_t		tn_aborted;
	trans_num		lcl_dirty;
	blk_hdr_ptr_t		encrypted_bp;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
#	ifdef DEBUG
	char			*in, *out;
	int			out_size, gtmcrypt_errno;
	static unsigned char	*private_blk;
	static int		private_blksz;
	unsigned int		lcl_t_tries;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 != gv_target->root);
	csa = cs_addrs;
	csd = csa->hdr;
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
		assert(bp->bsiz <= csd->blk_size);
		bsiz = MIN(bp->bsiz, csd->blk_size);
		memcpy(ptr, bp, bsiz);
		if (NULL != encr_ptr)
		{	/* The caller requested the encrypted buffer corresponding to `bp' as well. Take a copy of the encrypted
			 * twin global buffer. But do that only if the cache record corresponding to `bp' is not dirty and we know
			 * that the block is encrypted using the same settings as requested. Otherwise, either the encrypted twin
			 * remains stale (as of dsk_read) until a wcs_wtstart happens, at which point it updates the twin global
			 * buffer to contain the up-to-date encrypted contents; or we will need to first decrypt the block before
			 * reencrypting it anyway, so we might as well simply work with the unencrypted block directly.
			 */
			if (0 == (lcl_dirty = bh->cr->dirty))
			{
				if (NEEDS_NEW_KEY(csd, bp->tn))
				{	/* The block is encrypted with the new key, which defaults to non-null IVs. Hence, only
					 * return the encrypted buffer if it is OK to have non-null IVs in the extract.
					 */
					if (!use_null_iv)
					{
						encrypted_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, cs_addrs);
						memcpy(encr_ptr, (sm_uc_ptr_t)encrypted_bp, bsiz);
						*got_encrypted_block = ENCRYPTED_WITH_HASH2;
					} else
						*got_encrypted_block = NEEDS_ENCRYPTION;
				} else if (IS_ENCRYPTED(csd->is_encrypted))
				{	/* The block is encrypted with the old key, so only return the encrypted buffer if its
					 * null-IV setting is consistent with what we can have in the extract.
					 */
					if (use_null_iv != csd->non_null_iv)
					{
						encrypted_bp = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(bp, cs_addrs);
						memcpy(encr_ptr, (sm_uc_ptr_t)encrypted_bp, bsiz);
						*got_encrypted_block = ENCRYPTED_WITH_HASH1;
					} else
						*got_encrypted_block = NEEDS_ENCRYPTION;
				} else
				{	/* Block is not encrypted at all. */
					*got_encrypted_block = NEEDS_NO_ENCRYPTION;
				}
			} else
			{	/* Cache record is dirty, so encrypt the block, if needed, in the caller. */
				*got_encrypted_block = NEEDS_ENCRYPTION;
			}
		}
#		ifdef DEBUG
		lcl_t_tries = t_tries;
#		endif
		if ((trans_num)0 != t_end(&gv_target->hist, two_histories ? rt_history : NULL, TN_NOT_SPECIFIED))
		{	/* Transaction succeeded. */
			if (two_histories)
				memcpy(gv_target->hist.h, rt_history->h, SIZEOF(srch_blk_status) * (rt_history->depth + 1));
#			ifdef DEBUG
			if ((NULL != encr_ptr)
					&& ((ENCRYPTED_WITH_HASH1 == *got_encrypted_block)
						|| (ENCRYPTED_WITH_HASH2 == *got_encrypted_block)))
			{	/* Ensure that the copy of the encrypted twin global buffer we took before t_end is not stale. */
				if ((0 == private_blksz) || (private_blksz < csd->blk_size))
				{	/* (Re)allocate space for doing out-of-place encryption. */
					if (NULL != private_blk)
						free(private_blk);
					private_blksz = csd->blk_size;
					private_blk = (unsigned char *)malloc(private_blksz);
				}
				memcpy(private_blk, ptr, SIZEOF(blk_hdr));
				in = (char *)ptr + SIZEOF(blk_hdr);
				out = (char *)private_blk + SIZEOF(blk_hdr);
				out_size = bsiz - SIZEOF(blk_hdr);
				if (ENCRYPTED_WITH_HASH2 == *got_encrypted_block)
				{
					assert(GTMCRYPT_INVALID_KEY_HANDLE != csa->encr_key_handle2);
					GTMCRYPT_ENCRYPT(cs_addrs, TRUE, cs_addrs->encr_key_handle2,
							in, out_size, out, ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
				} else
				{
					assert(GTMCRYPT_INVALID_KEY_HANDLE != csa->encr_key_handle);
					GTMCRYPT_ENCRYPT(cs_addrs, csd->non_null_iv, cs_addrs->encr_key_handle,
							in, out_size, out, ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
				}
				assert(0 == gtmcrypt_errno);
				assert(0 == memcmp(private_blk, encr_ptr, bsiz));
			}
#			endif
			return !end_of_tree;
		} else
		{
			ABORT_TRANS_IF_GBL_EXIST_NOMORE(lcl_t_tries, tn_aborted);
			if (tn_aborted)
				return FALSE; /* global doesn't exist any more in the database */
		}
	}
}
