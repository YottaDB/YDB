/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "gvcst_protos.h"

#define INTEG_ERROR_RETURN											\
{														\
	gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_EXTRFAIL, 2, GNAME(gl_ptr).len, GNAME(gl_ptr).addr);	\
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
#ifdef GTM_CRYPT
GBLREF	mstr			pvt_crypt_buf;
#endif

error_def(ERR_EXTRFAIL);
error_def(ERR_RECORDSTAT);

#if defined(GTM_CRYPT)
boolean_t mu_extr_gblout(glist *gl_ptr, mu_extr_stats *st, int format, boolean_t is_any_file_encrypted)
#elif defined(UNIX)
boolean_t mu_extr_gblout(glist *gl_ptr, mu_extr_stats *st, int format)
#endif
{
	static gv_key			*beg_gv_currkey; 	/* this is used to check key out of order condition */
	static int			max_zwr_len = 0, index;
	static unsigned char		*private_blk = NULL, *zwr_buffer = NULL, *key_buffer = NULL;
	static uint4			private_blksz = 0;
	unsigned char			*cp2, current, *keytop, last;
	unsigned short			out_size, rec_size;
	int				data_len, des_len, fmtd_key_len, gname_size;
	int				tmp_cmpc;
	blk_hdr_ptr_t			bp;
	boolean_t			beg_key;
	rec_hdr_ptr_t			rp, save_rp;
	sm_uc_ptr_t			blktop, cp1, rectop, out;
	mval				*val_span = NULL;
	boolean_t			is_hidden, found_dummy = FALSE;
	blk_hdr_ptr_t			encrypted_bp;
#	ifdef GTM_CRYPT
	static sgmnt_data_ptr_t		prev_csd;
	gd_region			*reg, *reg_top;
#	endif

	if (0 == gv_target->root)
		return TRUE; /* possible if ROLLBACK ended up physically removing a global from the database */
	if (NULL == key_buffer)
		key_buffer = (unsigned char *)malloc(MAX_ZWR_KEY_SZ);
	if (ZWR_EXP_RATIO(cs_addrs->hdr->max_rec_size) > max_zwr_len)
	{
		if (NULL != zwr_buffer)
			free (zwr_buffer);
		max_zwr_len = ZWR_EXP_RATIO(cs_addrs->hdr->max_rec_size);
		zwr_buffer = (unsigned char *)malloc(MAX_ZWR_KEY_SZ + max_zwr_len);
	}
	assert(0 < cs_data->blk_size);
	if (cs_data->blk_size > private_blksz)
	{
		if (NULL != private_blk)
			free(private_blk);
		private_blksz = cs_data->blk_size;
		private_blk = (unsigned char *)malloc(private_blksz);
	}
	if (NULL == beg_gv_currkey)
		beg_gv_currkey = (gv_key *)malloc(SIZEOF(gv_key) + MAX_KEY_SZ);
	memcpy(beg_gv_currkey->base, gv_currkey->base, (SIZEOF(gv_key) + gv_currkey->end));
	gname_size = gv_currkey->end;
	keytop = &gv_currkey->base[gv_currkey->top];
	st->recknt = st->reclen = st->keylen = st->datalen = 0;
#	ifdef GTM_CRYPT
	if (is_any_file_encrypted && (format == MU_FMT_BINARY))
	{
		if (cs_data->is_encrypted)
		{
			ASSERT_ENCRYPTION_INITIALIZED;	/* due to op_gvname_fast done from gv_select in mu_extract */
			if (prev_csd != cs_data)
			{
				prev_csd = cs_data;
				index = find_reg_hash_idx(gv_cur_region);
			}
			/* We have to write the encrypted version of the block. Instead of encrypting the plain-text version of the
			 * block, we just reference the encrypted version of the block that is already maintained in sync with the
			 * plain-text version by wcs_wtstart and dsk_read (called eventually by mu_extr_getblk below). All we need
			 * to make sure is that we have a private buffer allocated (of appropriate size) in which mu_extr_getblk can
			 * return the encrypted version of the block. Do the allocation here.
			 */
			REALLOC_CRYPTBUF_IF_NEEDED(cs_data->blk_size);
		} else
		{	/* Encryption handle index of -1 indicates in an extract that the block is unencrypted. It is useful when
			 * the extract contains a mix of encrypted and unencrypted data.
			 */
			index = -1;
		}
	}
#	endif
	for ( ; ; )
	{
		if (mu_ctrly_occurred)
			return FALSE;
		if (mu_ctrlc_occurred)
		{
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_RECORDSTAT, 6, LEN_AND_LIT("TOTAL"),
				 &st->recknt, st->keylen, st->datalen, st->reclen);
			mu_ctrlc_occurred = FALSE;
		}
		encrypted_bp = NULL;
#		ifdef GTM_CRYPT
		if (cs_data->is_encrypted && (MU_FMT_BINARY == format))
			encrypted_bp = (blk_hdr_ptr_t)pvt_crypt_buf.addr;
#		endif
		if (!mu_extr_getblk(private_blk, (unsigned char *)encrypted_bp))
			break;
		bp = (blk_hdr_ptr_t)private_blk;
		if (bp->bsiz == SIZEOF(blk_hdr))
			break;
		if (0 != bp->levl || bp->bsiz < SIZEOF(blk_hdr) || bp->bsiz > cs_data->blk_size ||
				gv_target->hist.h[0].curr_rec.match < gname_size)
			INTEG_ERROR_RETURN
		blktop = (sm_uc_ptr_t)bp + bp->bsiz;
		if (format == MU_FMT_BINARY)
		{	/* At this point, gv_target->hist.h[0].curr_rec.offset points to the offset within the block at which
			 * the desired record exists. If this record is *not* the first record in the block (possible due to
			 * concurrent updates), the compression count for that record would be non-zero which means we cannot
			 * initiate a write to the extract file starting from this offset as the 'mupip load' command would
			 * consider this record as corrupted. So, we write the entire block instead. This could increase the
			 * size of the binary extract file, but the alternative is to expand the curent record and with encryption
			 * it becomes a performance overhead as we have to encrypt only the tail of a block. If we choose to
			 * write the whole block, we avoid encryption altogether because we have access to the encrypted block
			 * from the encrypted twin buffer.
			 */
			rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)bp + SIZEOF(blk_hdr));
			out_size = blktop - (sm_uc_ptr_t)rp;
			out = (sm_uc_ptr_t)rp;
#			ifdef GTM_CRYPT
			if (cs_data->is_encrypted)
			{
				assert(NULL != encrypted_bp);
				assert(encrypted_bp->bsiz == bp->bsiz);
				assert(encrypted_bp->tn == bp->tn);
				assert(encrypted_bp->levl == bp->levl);
				assert(out_size == (encrypted_bp->bsiz - SIZEOF(blk_hdr)));
				out = (sm_uc_ptr_t)encrypted_bp + SIZEOF(blk_hdr);
				assert(-1 != index);
			}
			WRITE_BIN_EXTR_BLK(out, out_size,
				is_any_file_encrypted ? WRITE_ENCR_HANDLE_INDEX_TRUE : WRITE_ENCR_HANDLE_INDEX_FALSE, index);
#			else
			index = -1;
			WRITE_BIN_EXTR_BLK(out, out_size, WRITE_ENCR_HANDLE_INDEX_FALSE, index);
#			endif
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
				INTEG_ERROR_RETURN
			cp1 = (sm_uc_ptr_t)(rp + 1);
			cp2 = gv_currkey->base + tmp_cmpc;
			if (cp2 >= keytop || cp1 >= rectop)
				INTEG_ERROR_RETURN
			if (!beg_key && (*cp2 >= *cp1))
				INTEG_ERROR_RETURN
			for (;;)
			{
				if (0 == (*cp2++ = *cp1++))
				{
					if (cp2 >= keytop || cp1 >= rectop)
						INTEG_ERROR_RETURN
					if (0 == (*cp2++ = *cp1++))
						break;
				}
				if (cp2 >= keytop || cp1 >= rectop)
					INTEG_ERROR_RETURN
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
				gvcst_get(val_span);
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
				INTEG_ERROR_RETURN
			if (st->datalen < data_len)
				st->datalen = data_len;
		} /* End scanning a block */
		if (((sm_uc_ptr_t)rp != blktop)
				|| (0 > memcmp(gv_currkey->base, beg_gv_currkey->base, MIN(gv_currkey->end, beg_gv_currkey->end))))
			INTEG_ERROR_RETURN;
		GVKEY_INCREMENT_QUERY(gv_currkey);
	} /* end outmost for */
	return TRUE;
}
