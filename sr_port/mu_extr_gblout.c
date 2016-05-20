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

#ifdef UNIX
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtmio.h"
#elif defined(VMS)
#include <rms.h>
#include <ssdef.h>
#include "iormdef.h"
#endif
#include "gtm_string.h"
#include "io.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "muextr.h"
#include "cdb_sc.h"
#include "copy.h"
#include "mlkdef.h"
#include "op.h"
#include "gvcst_expand_key.h"
#include "format_targ_key.h"
#include "zshow.h"
#include "gtmmsg.h"
#include "min_max.h"
#include "gtmcrypt.h"
#include "gvcst_protos.h"
#include "wbox_test_init.h"

#define INTEG_ERROR_RETURN(CSA)											\
{														\
	gtm_putmsg_csa(CSA_ARG(CSA) VARLSTCNT(4) ERR_EXTRFAIL, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);	\
	return FALSE;												\
}

#define WRITE_ENCR_HANDLE_INDEX_TRUE	TRUE
#define WRITE_ENCR_HANDLE_INDEX_FALSE	FALSE

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	mstr			pvt_crypt_buf;

error_def(ERR_EXTRFAIL);
error_def(ERR_RECORDSTAT);

boolean_t mu_extr_gblout(glist *gl_ptr, mu_extr_stats *st, int format, boolean_t any_file_encrypted,
		boolean_t any_file_uses_non_null_iv, int hash1_index, int hash2_index, boolean_t use_null_iv)
{
	static gv_key			*beg_gv_currkey; 	/* this is used to check key out of order condition */
	static int			max_zwr_len, index;
	static unsigned char		*private_blk, *zwr_buffer, *key_buffer;
	static uint4			private_blksz;
	unsigned char			*cp2, current, *keytop, last;
	unsigned short			out_size, rec_size;
	int				data_len, des_len, fmtd_key_len, gname_size;
	int				tmp_cmpc;
	blk_hdr_ptr_t			bp;
	boolean_t			beg_key;
	rec_hdr_ptr_t			rp, save_rp;
	sm_uc_ptr_t			blktop, cp1, rectop, out;
	mval				*val_span;
	boolean_t			is_hidden, found_dummy = FALSE;
	blk_hdr_ptr_t			encrypted_bp;
	sgmnt_data_ptr_t		csd;
	sgmnt_addrs			*csa;
	gd_region			*reg, *reg_top;
	gd_segment			*seg;
	int				gtmcrypt_errno, got_encrypted_block;
	int				wb_counter = 1;

	max_zwr_len = private_blksz = 0;
	private_blk = zwr_buffer = key_buffer = NULL;
	val_span = NULL;
	if (0 == gv_target->root)
		return TRUE; /* possible if ROLLBACK ended up physically removing a global from the database */
	csa = cs_addrs;
	csd = cs_data;
	if (NULL == key_buffer)
		key_buffer = (unsigned char *)malloc(MAX_ZWR_KEY_SZ);
	if (ZWR_EXP_RATIO(csd->max_rec_size) > max_zwr_len)
	{
		if (NULL != zwr_buffer)
			free(zwr_buffer);
		max_zwr_len = ZWR_EXP_RATIO(csd->max_rec_size);
		zwr_buffer = (unsigned char *)malloc(MAX_ZWR_KEY_SZ + max_zwr_len);
	}
	assert(0 < csd->blk_size);
	if (csd->blk_size > private_blksz)
	{
		if (NULL != private_blk)
			free(private_blk);
		private_blksz = csd->blk_size;
		private_blk = (unsigned char *)malloc(private_blksz);
	}
	if (NULL == beg_gv_currkey)
		beg_gv_currkey = (gv_key *)malloc(SIZEOF(gv_key) + MAX_KEY_SZ);
	memcpy(beg_gv_currkey->base, gv_currkey->base, (SIZEOF(gv_key) + gv_currkey->end));
	gname_size = gv_currkey->end;
	keytop = &gv_currkey->base[gv_currkey->top];
	MU_EXTR_STATS_INIT(*st);
	if (any_file_encrypted && (format == MU_FMT_BINARY))
	{
		ASSERT_ENCRYPTION_INITIALIZED;	/* due to op_gvname_fast done from gv_select in mu_extract */
		/* Encryption handle index of -1 indicates in an extract that the block is unencrypted. It is useful when
		 * the extract contains a mix of encrypted and unencrypted data.
		 */
		assert((-1 == hash1_index) || IS_ENCRYPTED(csd->is_encrypted));
		assert((-1 == hash2_index) || USES_NEW_KEY(csd));
		/* We have to write the encrypted version of the block. Depending on the type of the extract, we may either
		 * need to reencrypt the block using the null iv or just reference the encrypted version of the block that
		 * is already maintained in sync with the plain-text version by wcs_wtstart and dsk_read (called eventually
		 * by mu_extr_getblk below). In either case we need to make sure that we have a big enough private buffer
		 * allocated in which to store the encrypted version of the block. Only if we are going to use the iv, make
		 * room for it also to avoid allocating separate memory. Do the allocation here.
		 */
		REALLOC_CRYPTBUF_IF_NEEDED(csd->blk_size);
	}
	for ( ; ; )
	{
		if (mu_ctrly_occurred)
			return FALSE;
		if (mu_ctrlc_occurred)
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_RECORDSTAT, 6, LEN_AND_LIT("TOTAL"),
				 &st->recknt, st->keylen, st->datalen, st->reclen);
			mu_ctrlc_occurred = FALSE;
		}
		if ((MU_FMT_BINARY == format) && ((-1 != hash1_index) || (-1 != hash2_index)))
			encrypted_bp = (blk_hdr_ptr_t)pvt_crypt_buf.addr;
		else
			encrypted_bp = NULL;
		if (!mu_extr_getblk(private_blk, (unsigned char *)encrypted_bp, use_null_iv, &got_encrypted_block))
			break;
		bp = (blk_hdr_ptr_t)private_blk;
		if (bp->bsiz == SIZEOF(blk_hdr))
			break;
		if (0 != bp->levl || bp->bsiz < SIZEOF(blk_hdr) || bp->bsiz > csd->blk_size ||
				gv_target->hist.h[0].curr_rec.match < gname_size)
			INTEG_ERROR_RETURN(csa);
		blktop = (sm_uc_ptr_t)bp + bp->bsiz;
		if (MU_FMT_BINARY == format)
		{	/* At this point, gv_target->hist.h[0].curr_rec.offset points to the offset within the block at which the
			 * desired record exists. If this record is *not* the first record in the block (possible due to concurrent
			 * updates), the compression count for that record would be non-zero which means we cannot initiate a write
			 * to the extract file starting from this offset as the 'mupip load' command would consider this record as
			 * corrupted. So, we write the entire block instead. This could increase the size of the binary extract
			 * file, but the alternative is to expand the curent record and with encryption it becomes a performance
			 * overhead as we have to encrypt only the tail of a block. If we choose to write the whole block, we avoid
			 * encryption altogether because we have access to the encrypted block from the encrypted twin buffer.
			 */
			rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)bp + SIZEOF(blk_hdr));
			out_size = blktop - (sm_uc_ptr_t)rp;
			out = (sm_uc_ptr_t)rp;

			if (NULL != encrypted_bp)
			{
				switch (got_encrypted_block)
				{
					case ENCRYPTED_WITH_HASH1:
						assert(encrypted_bp->bsiz == bp->bsiz);
						assert(encrypted_bp->tn == bp->tn);
						assert(encrypted_bp->levl == bp->levl);
						assert(-1 != hash1_index);
						index = hash1_index;
						out = (sm_uc_ptr_t)encrypted_bp;
						break;
					case ENCRYPTED_WITH_HASH2:
						assert(encrypted_bp->bsiz == bp->bsiz);
						assert(encrypted_bp->tn == bp->tn);
						assert(encrypted_bp->levl == bp->levl);
						assert(-1 != hash2_index);
						index = hash2_index;
						out = (sm_uc_ptr_t)encrypted_bp;
						break;
					case NEEDS_ENCRYPTION:
						if ((-1 != hash2_index) && (csd->encryption_hash2_start_tn <= bp->tn))
						{
							assert(GTMCRYPT_INVALID_KEY_HANDLE != csa->encr_key_handle2);
							index = hash2_index;
							GTMCRYPT_ENCRYPT(csa, !use_null_iv, csa->encr_key_handle2, rp, out_size,
									encrypted_bp + 1, bp, SIZEOF(blk_hdr), gtmcrypt_errno);
						} else if (-1 != hash1_index)
						{
							assert(GTMCRYPT_INVALID_KEY_HANDLE != csa->encr_key_handle);
							index = hash1_index;
							GTMCRYPT_ENCRYPT(csa, !use_null_iv, csa->encr_key_handle, rp, out_size,
									encrypted_bp + 1, bp, SIZEOF(blk_hdr), gtmcrypt_errno);
						} else
						{
							assert(FALSE);
						}
						memcpy(encrypted_bp, bp, SIZEOF(blk_hdr));
						if (0 != gtmcrypt_errno)
						{
							seg = csa->region->dyn.addr;
							GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error,
									seg->fname_len, seg->fname);
						}
						out = (sm_uc_ptr_t)encrypted_bp;
						break;
					case NEEDS_NO_ENCRYPTION:
						/* This path is possible if we are dealing with an unencrypted database that is
						 * being encrypted, and thus index1 = -1 while index 2 != -1, and this particular
						 * block has not been encrypted yet.
						 */
						index = -1;
						break;
					default:
						assert(FALSE);
				}
				if (-1 != index)
				{	/* For non-null IVs we need to write the entire encrypted block. */
					if (any_file_uses_non_null_iv)
						out_size = encrypted_bp->bsiz;
					else
						out = (sm_uc_ptr_t)((blk_hdr *)out + 1);
				}
			} else
				index = -1;
			WRITE_BIN_EXTR_BLK(out, out_size,
				any_file_encrypted ? WRITE_ENCR_HANDLE_INDEX_TRUE : WRITE_ENCR_HANDLE_INDEX_FALSE, index);
		} else
		{	/* Note that rp may not be the beginning of a block */
			rp = (rec_hdr_ptr_t)(gv_target->hist.h[0].curr_rec.offset + (sm_uc_ptr_t)bp);
		}
		for (beg_key = TRUE; (sm_uc_ptr_t)rp < blktop; rp = (rec_hdr_ptr_t)rectop)
		{ 	/* Start scanning a block */
			GET_USHORT(rec_size, &rp->rsiz);
			rectop = (sm_uc_ptr_t)rp + rec_size;
			EVAL_CMPC2(rp, tmp_cmpc);
			if (rectop > blktop || tmp_cmpc > gv_currkey->end ||
				(((unsigned char *)rp != private_blk + SIZEOF(blk_hdr)) && (tmp_cmpc < gname_size)))
				INTEG_ERROR_RETURN(csa);
			cp1 = (sm_uc_ptr_t)(rp + 1);
			cp2 = gv_currkey->base + tmp_cmpc;
			if (cp2 >= keytop || cp1 >= rectop)
				INTEG_ERROR_RETURN(csa);
			if (!beg_key && (*cp2 >= *cp1))
				INTEG_ERROR_RETURN(csa);
			for (;;)
			{
				if (0 == (*cp2++ = *cp1++))
				{
					if (cp2 >= keytop || cp1 >= rectop)
						INTEG_ERROR_RETURN(csa);
					if (0 == (*cp2++ = *cp1++))
						break;
				}
				if (cp2 >= keytop || cp1 >= rectop)
					INTEG_ERROR_RETURN(csa);
			}
			gv_currkey->end = cp2 - gv_currkey->base - 1;
			if (beg_key)
			{ 	/* beg_gv_currkey usually the first key of a block,
				   but for concurrency conflict it could be any key */
				beg_key = FALSE;
				memcpy(beg_gv_currkey->base, gv_currkey->base, gv_currkey->end + 1);
				beg_gv_currkey->end = gv_currkey->end;
			}
			if (st->reclen < rec_size)
				st->reclen = rec_size;
#			ifdef UNIX
			CHECK_HIDDEN_SUBSCRIPT(gv_currkey, is_hidden);
			if (is_hidden)
				continue;
#			endif
			st->recknt++;
			if (st->keylen < gv_currkey->end + 1)
				st->keylen = gv_currkey->end + 1;
			data_len = (int)(rec_size - (cp1 - (sm_uc_ptr_t)rp));
#			ifdef UNIX
			if ((1 == data_len) && ('\0' == *cp1))
			{	/* Possibly (probably) a spanning node. Need to read in more blocks to get the value. Note: This
				 * additional gvcst_get is needed only for ZWR/GO extracts and not for BINARY extracts as the
				 * latter dumps the entire block content. But, we need to read the value anyways to report accurate
				 * statistics on the maximum data length (st->datalen). So, do the gvcst_get irrespective of
				 * whether this is a ZWR/GO/BINARY extract.
				 */
				if (!val_span)
				{	/* protect val_span from stp_gcol in WRITE_EXTR_LINE/op_write */
					PUSH_MV_STENT(MVST_MVAL);
					val_span = &mv_chain->mv_st_cont.mvs_mval;
				}
#				ifdef DEBUG
				if (gtm_white_box_test_case_enabled &&
					(WBTEST_MUEXTRACT_GVCST_RETURN_FALSE == gtm_white_box_test_case_number))
				{	/* white box case to simulate concurrent change, Sleeping for 10 seconds
					 * to kill the second variable.
					 */
					if (2 == wb_counter)
					{
						LONG_SLEEP(10);
					}
					wb_counter++;
				}
#				endif
				if (!gvcst_get(val_span))
				{
					val_span->mvtype = 0; /* so stp_gcol can free up any space */
					st->recknt--;
					continue;
				}
				cp1 = (unsigned char *)val_span->str.addr;
				data_len = val_span->str.len;
				found_dummy = TRUE;
			}
#			endif
			if (MU_FMT_BINARY != format)
			{
				cp2 = (unsigned char *)format_targ_key(key_buffer, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
				fmtd_key_len = (int)(cp2 - key_buffer);
				if (MU_FMT_ZWR == format)
				{
					memcpy(zwr_buffer, key_buffer, fmtd_key_len);
					memcpy(zwr_buffer + fmtd_key_len, "=", 1);
					des_len = 0;
					format2zwr(cp1, data_len, zwr_buffer + fmtd_key_len + 1, &des_len);
					WRITE_EXTR_LINE(zwr_buffer, (fmtd_key_len + des_len + 1));
				}
				else if (MU_FMT_GO == format)
				{
					WRITE_EXTR_LINE(key_buffer, fmtd_key_len);
					WRITE_EXTR_LINE(cp1, data_len);
				}
			}
#			ifdef UNIX
			if (found_dummy)
			{
				val_span->mvtype = 0; /* so stp_gcol can free up any space */
				found_dummy = FALSE;
			}
#			endif
			if (0 > data_len)
				INTEG_ERROR_RETURN(csa);
			if (st->datalen < data_len)
				st->datalen = data_len;
		} /* End scanning a block */
		if (((sm_uc_ptr_t)rp != blktop)
				|| (0 > memcmp(gv_currkey->base, beg_gv_currkey->base, MIN(gv_currkey->end, beg_gv_currkey->end))))
			INTEG_ERROR_RETURN(csa);
		GVKEY_INCREMENT_QUERY(gv_currkey);
	} /* end outmost for */
	return TRUE;
}
