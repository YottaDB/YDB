/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

#elif defined(VMS)
#include <rms.h>
#include <ssdef.h>

#else
#error UNSUPPORTED PLATFORM
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "hashdef.h"
#include "muextr.h"
#include "cdb_sc.h"
#include "copy.h"
#include "mlkdef.h"

#ifdef UNIX
#include "gtmio.h"
#include "val_print.h"

#elif defined(VMS)
#include "msg.h"
#include "outrab_print.h"

#else
#error UNSUPPORTED PLATFORM
#endif
#include "op.h"
#include "gvcst_expand_key.h"
#include "format_targ_key.h"
#include "zshow.h"

#define KEY_BUFF_SIZE       512
#define MAX_EXPAND_TIMES    7

GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;

static readonly unsigned char gt_lit[] = "EXTRACT TOTAL";

#ifdef UNIX
void mu_extr_gblout(mval *gn, mu_extr_stats *st, int format)
#elif defined(VMS)
void mu_extr_gblout(mval *gn, struct RAB *outrab, mu_extr_stats *st, int format)
#else
#error UNSUPPORTED PLATFORM
#endif
{
	int		status, gname_size, n, src_len, des_len, max_zwr_len, fmtd_key_len;
	short		out_size;
	unsigned short	rec_size;
	blk_hdr_ptr_t 	bp;
	rec_hdr_ptr_t 	rp, toprec;
	sm_uc_ptr_t 	cp1, src;
	unsigned char  	*key_buffer, *des, *cp2, *zwr_buffer;
	bool 		done;
	static unsigned char	*private_blk = NULL;
	static uint4	private_blksz = 0;
#ifdef UNIX
	mval		val;
#elif defined(VMS)
	msgtype		*msg;
	error_def(ERR_KEYCNT);
	error_def(ERR_BINSTATS);
	msg = malloc(sizeof(msgtype) + 2 * FAO_ARG);
#else
#error UNSUPPORTED PLATFORM
#endif

	max_zwr_len = KEY_BUFF_SIZE + MAX_EXPAND_TIMES * cs_addrs->hdr->max_rec_size + 1;
	zwr_buffer = (unsigned char *)malloc(max_zwr_len);
	key_buffer = (unsigned char *)malloc(KEY_BUFF_SIZE);
	assert(0 < cs_data->blk_size);
	if (cs_data->blk_size > private_blksz)
	{
		if (NULL != private_blk)
			free(private_blk);
		private_blk = (unsigned char *)malloc(cs_data->blk_size);
		private_blksz = cs_data->blk_size;
	}
	st->recknt = st->reclen = st->keylen = st->datalen = 0;
	op_gvname(VARLSTCNT(1) gn);
	gname_size = gv_currkey->end;
	for (done = FALSE ; !done ; )
	{
		memset(private_blk, 0, private_blksz);
		if (!mu_extr_getblk(private_blk))
			break;
		bp = (blk_hdr_ptr_t)private_blk;
		assert (0 == bp->levl);
		if (bp->bsiz <= sizeof(blk_hdr))
			continue;
		if (gv_target->hist.h[0].curr_rec.match < gname_size)
			break;
		rp = (rec_hdr_ptr_t)(gv_target->hist.h[0].curr_rec.offset + (sm_uc_ptr_t) bp);
		toprec = (rec_hdr_ptr_t)(bp->bsiz + (sm_uc_ptr_t) bp);
		cp1 = (sm_uc_ptr_t)(rp + 1);
		if (format == MU_FMT_BINARY)
		{
			if (mu_ctrly_occurred)
			{
			        free(key_buffer);
				free(zwr_buffer);
#ifdef VMS
				free(msg);
#endif
				return;
			}
			if (mu_ctrlc_occurred)
			{
#ifdef UNIX
				PRINTF("%s\t Key Cnt: %d  max rec size: %d\n",
					gt_lit, st->recknt, st->reclen);
#elif defined(VMS)
				MSG_PRINT(6, 1, ERR_BINSTATS, st->recknt, st->reclen, 0, 0, 0);
#else
#error UNSUPPORTED PLATFORM
#endif
				mu_ctrlc_occurred = FALSE;
			}
			assert ((sm_uc_ptr_t) rp == (sm_uc_ptr_t) bp + sizeof (blk_hdr));
			out_size = (sm_uc_ptr_t) toprec - (sm_uc_ptr_t) rp;
#ifdef UNIX
			VAL_PRINT(MV_STR, (char *)(&out_size), sizeof(out_size));
			/* output records of current block */
			VAL_PRINT(MV_STR, (char *)rp, out_size);
#elif defined(VMS)
			OUTRAB_PRINT(outrab, (unsigned char *) rp, out_size);
#endif
			GET_USHORT(rec_size, &rp->rsiz);
			while ((rec_hdr_ptr_t)((sm_uc_ptr_t) rp + rec_size) < toprec)
			{
				st->recknt++;
				if (st->reclen < rec_size)
					st->reclen = rec_size;
				rp = (rec_hdr_ptr_t)((sm_uc_ptr_t) rp + rec_size);
				GET_USHORT(rec_size, &rp->rsiz);
			}
			st->recknt++;
			if (st->reclen < rec_size)
				st->reclen = rec_size;
			assert ((rec_hdr_ptr_t)((sm_uc_ptr_t) rp + rec_size) == toprec);
			if (gvcst_expand_key (bp, (sm_uc_ptr_t) rp - (sm_uc_ptr_t) bp, gv_currkey)
				!= cdb_sc_normal)
				break;
			if (st->keylen < gv_currkey->end)
				st->keylen = gv_currkey->end;
			n = rec_size - (gv_currkey->end - rp->cmpc) - sizeof(rec_hdr) - 1;
			if (st->datalen < n)
				st->datalen = n;

			gv_currkey->base[gv_currkey->end] = 1;
			gv_currkey->base[gv_currkey->end + 1] = 0;
			gv_currkey->base[gv_currkey->end + 2] = 0;
			gv_currkey->end += 2;

		}
		else
		{
			assert((MU_FMT_ZWR == format) || (MU_FMT_GO == format));
			/** ONLY NEED TO COPY AFTER MATCH **/
			cp2 = gv_currkey->base + rp->cmpc;
			for (;;)
				if ((*cp2++ = *cp1++) == 0)
					if ((*cp2++ = *cp1++) == 0)
						break;
			gv_currkey->end = cp2 - gv_currkey->base - 1;
			for (;;)
			{
				if (mu_ctrly_occurred)
				{
				        free(key_buffer);
					free(zwr_buffer);
#ifdef VMS
					free(msg);
#endif
					return;
				}
				if (mu_ctrlc_occurred)
				{
#ifdef UNIX
					PRINTF("%s\t Key Cnt: %d  max subsc len: %d  max data len: %d  max rec len: %d\n",
						gt_lit, st->recknt, st->keylen, st->datalen, st->reclen);
#elif defined(VMS)
					MSG_PRINT(8, 1, ERR_KEYCNT, 6, st->recknt, st->keylen, st->datalen, st->reclen);
#endif
					mu_ctrlc_occurred = FALSE;
				}
				st->recknt++;
				n = gv_currkey->end;
				if (st->keylen < n)
					st->keylen = n;
				cp2 = (unsigned char *)format_targ_key(key_buffer, KEY_BUFF_SIZE, gv_currkey, TRUE);
				fmtd_key_len = cp2 - key_buffer;
				GET_USHORT(rec_size, &rp->rsiz);
				if (st->reclen < rec_size)
					st->reclen = rec_size;
				n = rec_size - (cp1 - (sm_uc_ptr_t) rp);
				if (MU_FMT_ZWR == format)
				{
					memcpy(zwr_buffer, key_buffer, fmtd_key_len);
					memcpy(zwr_buffer + fmtd_key_len, "=", 1);
					src = cp1;
					src_len = n;
					des_len = 0;
					des = zwr_buffer + fmtd_key_len + 1;
					format2zwr(src, src_len, des, &des_len);
					if (st->datalen < src_len)
						st->datalen = src_len;
#ifdef UNIX
					VAL_PRINT(MV_STR, (char *)zwr_buffer, (fmtd_key_len + des_len + 1));
					op_wteol(1);
#elif defined(VMS)
					OUTRAB_PRINT2(outrab, zwr_buffer, (fmtd_key_len + des_len + 1));
#endif
				}
				else
				{
				        if (st->datalen < n)
					        st->datalen = n;
#ifdef UNIX
					VAL_PRINT(MV_STR, (char *)key_buffer, fmtd_key_len);
					op_wteol(1);
					VAL_PRINT(MV_STR, (char *)cp1, n);
					op_wteol(1);
#elif defined(VMS)
					OUTRAB_PRINT2(outrab, key_buffer, fmtd_key_len);
					OUTRAB_PRINT2(outrab, cp1, n);
#endif
				}
				rp = (rec_hdr_ptr_t)((sm_uc_ptr_t) rp + rec_size);
				if (rp >= toprec)
					break;
				cp1 = (sm_uc_ptr_t)(rp + 1);
				cp2 = gv_currkey->base + rp->cmpc;
				for (;;)
					if ((*cp2++ = *cp1++) == 0)
						if ((*cp2++ = *cp1++) == 0)
							break;
				gv_currkey->end = cp2 - gv_currkey->base - 1;
				if (rp->cmpc < gname_size)
				{
					done = TRUE;
					break;
				}
			}
			cp1 = &gv_currkey->base[gv_currkey->end];
			gv_currkey->end += 2;
			*cp1++ = 1;
			*cp1++ = 0;
			*cp1++ = 0;
		}
	}
#ifdef VMS
	free(msg);
#endif
	free(key_buffer);
	free(zwr_buffer);
	return;
}
