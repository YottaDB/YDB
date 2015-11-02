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

#ifdef UNIX
#include <errno.h>
#include "gtm_stdio.h"
#include "gtmio.h"
#elif defined(VMS)
#include <rms.h>
#include <ssdef.h>
#include "iormdef.h"
#else
#error UNSUPPORTED PLATFORM
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

#define INTEG_ERROR_RETURN 							\
{										\
	gtm_putmsg(VARLSTCNT(4) ERR_EXTRFAIL, 2, gn->str.len, gn->str.addr); 	\
	return FALSE;								\
}

GBLREF	bool			mu_ctrlc_occurred;
GBLREF	bool			mu_ctrly_occurred;
GBLREF	gd_addr			*gd_header;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

error_def(ERR_EXTRFAIL);
error_def(ERR_RECORDSTAT);

STATICDEF readonly unsigned char gt_lit[] = "TOTAL";

#if defined(UNIX) && defined(GTM_CRYPT)
boolean_t mu_extr_gblout(mval *gn, mu_extr_stats *st, int format, muext_hash_hdr_ptr_t hash_array,
							boolean_t is_any_file_encrypted)
#elif defined(UNIX)
boolean_t mu_extr_gblout(mval *gn, mu_extr_stats *st, int format)
#elif defined(VMS)
boolean_t mu_extr_gblout(mval *gn, struct RAB *outrab, mu_extr_stats *st, int format)
#else
#error UNSUPPORTED PLATFORM
#endif
{
	blk_hdr_ptr_t 	bp;
	boolean_t 	beg_key;
	int		data_len, des_len, fmtd_key_len, gname_size;
	rec_hdr_ptr_t 	rp, save_rp;
	sm_uc_ptr_t 	blktop, cp1, rectop;
	unsigned char  	*cp2, current, *keytop, last;
	unsigned short	out_size, rec_size;
	static gv_key	*beg_gv_currkey; 	/* this is used to check key out of order condition */
	static int	max_zwr_len = 0;
	static unsigned char	*private_blk = NULL, *zwr_buffer = NULL, *key_buffer = NULL;
	static uint4	private_blksz = 0;

#	ifdef GTM_CRYPT
	char				*inbuf;
	gd_region			*reg, *reg_top;
	int				crypt_status, init_status;
	static gtmcrypt_key_t		encr_key_handle;
	static int4			index, prev_allocated_size;
	static sgmnt_data_ptr_t		prev_csd;
	static unsigned char		*unencrypted_blk_buff;
#	endif

	op_gvname(VARLSTCNT(1) gn);	/* op_gvname() must be done before any usage of cs_addrs or, gv_currkey */
	if (0 == gv_target->root)
		return TRUE; /* possible if ROLLBACK ended up physically removing a global from the database */
	if (NULL == key_buffer)
		key_buffer = (unsigned char *)malloc(MAX_ZWR_KEY_SZ);
	if (ZWR_EXP_RATIO(cs_addrs->hdr->max_rec_size) > max_zwr_len)
	{
		if (NULL != zwr_buffer)
			free (zwr_buffer);
		max_zwr_len = ZWR_EXP_RATIO(cs_addrs->hdr->max_rec_size);
		zwr_buffer = (unsigned char *)malloc(max_zwr_len);
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
	if (is_any_file_encrypted && (cs_data->is_encrypted) && (format == MU_FMT_BINARY))
	{
		INIT_PROC_ENCRYPTION(init_status);
		if (0 != init_status)
		{
			GC_GTM_PUTMSG(init_status, gv_cur_region->dyn.addr->fname);
			return FALSE;
		}
		if (prev_csd != cs_data)
		{
			prev_csd = cs_data;
			for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions, index = 0;
					reg < reg_top; reg++, index++)
				if (gv_cur_region == reg)
					break;
			assert(gv_cur_region < reg_top);
			GTMCRYPT_GETKEY(hash_array[index].gtmcrypt_hash, encr_key_handle, crypt_status);
			if (0 != crypt_status)
			{
				GC_GTM_PUTMSG(init_status, gv_cur_region->dyn.addr->fname);
				return FALSE;
			}
		}
	}
#	endif
	for ( ; ; )
	{
		if (mu_ctrly_occurred)
			return FALSE;
		if (mu_ctrlc_occurred)
		{
			gtm_putmsg(VARLSTCNT(8) ERR_RECORDSTAT, 6, LEN_AND_LIT(gt_lit),
				st->recknt, st->keylen, st->datalen, st->reclen);
			mu_ctrlc_occurred = FALSE;
		}
		if (!mu_extr_getblk(private_blk))
			break;
		bp = (blk_hdr_ptr_t)private_blk;
		if (bp->bsiz == SIZEOF(blk_hdr))
			break;
		if (0 != bp->levl || bp->bsiz < SIZEOF(blk_hdr) || bp->bsiz > cs_data->blk_size ||
				gv_target->hist.h[0].curr_rec.match < gname_size)
			INTEG_ERROR_RETURN
		/* Note that rp may not be the beginning of a block */
		rp = (rec_hdr_ptr_t)(gv_target->hist.h[0].curr_rec.offset + (sm_uc_ptr_t)bp);
		blktop = (sm_uc_ptr_t)bp + bp->bsiz;
		if (format == MU_FMT_BINARY)
		{
			out_size = blktop - (sm_uc_ptr_t)rp;
			save_rp = rp;
#			ifdef GTM_CRYPT
			if (is_any_file_encrypted)
			{
				/* Note that we are only encrypting data blocks */
				if (cs_data->is_encrypted)
				{
					inbuf = (char *)(rp);

					*(int4 *)(cs_addrs->encrypted_blk_contents) = index;
					GTMCRYPT_ENCODE_FAST(encr_key_handle,
								inbuf,
								out_size,
								cs_addrs->encrypted_blk_contents + SIZEOF(int4),
								crypt_status);
					if (0 != crypt_status)
					{
						GC_GTM_PUTMSG(crypt_status, gv_cur_region->dyn.addr->fname);
						return FALSE;
					}
				} else
				{
					/* If we extract from a mix of encrypted and unencrypted databases, for the unencrypted
					 * databases, cs_addrs->encrypted_blk_contents will not be initialized in db_init. Hence
					 * we use a static buffer for this purpose. */
					if (NULL == unencrypted_blk_buff || (prev_allocated_size < out_size))
					{
						if (NULL != unencrypted_blk_buff)
							free(unencrypted_blk_buff);
						unencrypted_blk_buff = (unsigned char *) malloc(out_size + SIZEOF(int4));
						prev_allocated_size = out_size;
					}
					*(int4 *)(unencrypted_blk_buff) = -1;
					memcpy(unencrypted_blk_buff + (SIZEOF(int4)), rp, out_size);
				}
				rp = (cs_data->is_encrypted) ? (rec_hdr_ptr_t)cs_addrs->encrypted_blk_contents
							     : (rec_hdr_ptr_t)unencrypted_blk_buff;
				out_size += SIZEOF(int4);
			}
#			endif
			WRITE_BIN_EXTR_BLK(rp, out_size); /* output records of current block */
			rp = save_rp;
		}
		for (beg_key = TRUE; (sm_uc_ptr_t)rp < blktop; rp = (rec_hdr_ptr_t)rectop)
		{ 	/* Start scanning a block */
			GET_USHORT(rec_size, &rp->rsiz);
			rectop = (sm_uc_ptr_t)rp + rec_size;
			if (rectop > blktop || rp->cmpc > gv_currkey->end ||
				(((unsigned char *)rp != private_blk + SIZEOF(blk_hdr)) && rp->cmpc < gname_size))
				INTEG_ERROR_RETURN
			cp1 = (sm_uc_ptr_t)(rp + 1);
			cp2 = gv_currkey->base + rp->cmpc;
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
			st->recknt++;
			if (st->reclen < rec_size)
				st->reclen = rec_size;
			if (st->keylen < gv_currkey->end + 1)
				st->keylen = gv_currkey->end + 1;
			data_len = (int)(rec_size - (cp1 - (sm_uc_ptr_t)rp));
			if (0 > data_len)
				INTEG_ERROR_RETURN
			if (st->datalen < data_len)
				st->datalen = data_len;
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
		} /* End scanning a block */
		if ((sm_uc_ptr_t)rp != blktop ||
			(memcmp(gv_currkey->base, beg_gv_currkey->base, MIN(gv_currkey->end, beg_gv_currkey->end)) < 0))
			INTEG_ERROR_RETURN
		gv_currkey->base[gv_currkey->end] = 1;
		gv_currkey->base[gv_currkey->end + 1] = 0;
		gv_currkey->base[gv_currkey->end + 2] = 0;
		gv_currkey->end += 2;
	} /* end outmost for */
	return TRUE;
}
