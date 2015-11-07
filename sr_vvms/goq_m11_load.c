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
#include <rms.h>
#include "efn.h"
#include <ssdef.h>
#include <iodef.h>
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "msg.h"
#include "muextr.h"
#include "stringpool.h"
#include "util.h"
#include "op.h"
#include "error.h"
#include "mu_load_stat.h"
#include "mvalconv.h"
#include "mu_gvis.h"
#include "quad2asc.h"

#define ODD 1
#define NEGORZRO 1
#define M11_ZRO 96
#define M11_EXP 64
#define M11_NEG 128
#define GTM_PREC 15

#define MVX_BLK_SIZE 2048
#define M11_BLK_SIZE 1024

GBLREF bool	mupip_error_occurred;
GBLREF bool	mupip_DB_full;
GBLREF bool	mu_ctrly_occurred;
GBLREF bool	mu_ctrlc_occurred;
GBLREF gd_addr	*gd_header;
GBLREF gv_key	*gv_currkey;
GBLREF spdesc stringpool;

#define US	0
#define THEM	1
#define GOQ_MAX_KSIZ 512
#define LCL_BUF_SIZE 256

error_def(ERR_BLKCNT);
error_def(ERR_CORRUPT);
error_def(ERR_GOQPREC);
error_def(ERR_LOADABORT);
error_def(ERR_LOADCTRLY);

void  goq_m11_load(struct FAB *infab, char *in_buff, uint4 rec_count, uint4 end)
{
	uint4		max_data_len, max_subsc_len, key_count, global_key_count;
	int		status;
	msgtype		msg;
	char  		digit,exp, k_buff[LCL_BUF_SIZE];
	unsigned char	*cp1, *cp2, *cp3, *dp1, *b, *btop;
	boolean_t	mupip_precision_error, is_dec, is_end;
	unsigned int	len, n, order;
	unsigned short  goq_blk_size;
	short		iosb[4];
	struct {
			unsigned char	cmpc;
			unsigned char	subsc_len;
			unsigned char	subsc[1];
		} 	*goq_rp;
	short int	*goq_blk_used;
	gv_key		*goq_currkey;
	int		goq_key_sub, goq_subsc_map[2][129];
	mval		v, exist;

	if (end > 0)
		is_end = TRUE;
	else 	is_end = FALSE;

	max_data_len = max_subsc_len = key_count = 0;
	goq_subsc_map[0][0] = goq_subsc_map[1][0] = 0;

	goq_blk_size = M11_BLK_SIZE;
	goq_currkey = malloc (SIZEOF(gv_key) + GOQ_MAX_KSIZ - 1);
	goq_currkey->top = GOQ_MAX_KSIZ;
	goq_blk_used = in_buff + goq_blk_size - SIZEOF(short int);
	gv_currkey = malloc (SIZEOF(gv_key) + GOQ_MAX_KSIZ - 1 + MAX_GVKEY_PADDING_LEN);
	gv_currkey->top = GOQ_MAX_KSIZ;


	lib$establish(mupip_load_ch);

	for (; !mupip_DB_full ;)
	{
		mupip_precision_error = FALSE;
		mupip_error_occurred = FALSE;
		if (mu_ctrly_occurred)
		{	break;
		}
		if (mu_ctrlc_occurred)
		{
			mu_load_stat(max_data_len, max_subsc_len, key_count, rec_count, ERR_BLKCNT);
			mu_gvis();
			util_out_print(0,TRUE);
		}
		/* reset the stringpool for every record in order to avoid garbage collection */
		stringpool.free = stringpool.base;
		if (is_end && (rec_count >= end))
			break;
		len = 0;
		/*****************************************************************************************/
		while (len == 0)
		{
			status = sys$qio(efn_bg_qio_read, infab->fab$l_stv ,IO$_READVBLK , &iosb[0],
					   0,0,in_buff,goq_blk_size,
					   (rec_count * goq_blk_size / 512) + 1,0,0,0);

			if (status != SS$_NORMAL)
				rts_error(VARLSTCNT(1) status);

			sys$synch(efn_bg_qio_read, &iosb[0]);
			if (iosb[0] == SS$_ENDOFFILE)
				break;

			if (iosb[0] != SS$_NORMAL)
				rts_error(VARLSTCNT(1) iosb[0]);
			rec_count++;
			len = *goq_blk_used;
		}
		/*****************************************************************************************/
		if ((is_end && rec_count >= end) || iosb[0] == SS$_ENDOFFILE)
			break;
		if (len >= goq_blk_size)
		{	rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,0);
			continue;
		}
		goq_rp = in_buff;

		btop = in_buff + len;
		global_key_count = 1;
		goq_key_sub = 0;

		b = v.str.addr = &k_buff[0];
		cp1 = &goq_rp->subsc[0];
		cp2 = &goq_rp->subsc[0] + goq_rp->subsc_len;
		cp3 = &goq_currkey->base[0];

		while (*cp1 & ODD)
		{
			*b++ = *cp1 /2;
			*cp3++ = *cp1++;
		}
		*b++ = *cp1 /2;
		*cp3++ = *cp1++;
		v.str.len = b - (unsigned char *) v.str.addr;

		if (goq_rp->cmpc != 0 || v.str.len > goq_currkey->top)
		{
			rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
			continue;
		}
		GV_BIND_NAME_AND_ROOT_SEARCH (gd_header, &v.str);
		if (mupip_error_occurred)
		{
			rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
			mu_gvis();
			util_out_print(0,TRUE);
			continue;
		}
		goq_key_sub++;
		goq_subsc_map [US][goq_key_sub] = gv_currkey->end;
		goq_subsc_map [THEM][goq_key_sub] = goq_currkey->end = v.str.len;

		while (cp1 < cp2)
		{
			*cp3++ = *cp1++;
		}
		cp3 = &goq_currkey->base[0];
		cp1 = cp3 + goq_currkey->end;
		cp2 = cp3 + goq_rp->subsc_len;
		for (;; )
		{
			dp1 = &goq_rp->subsc[ goq_rp->subsc_len ];
			n = *dp1 + SIZEOF(char); /* size of data */

			n += goq_rp->subsc_len + 2 * SIZEOF(char); /* size of key */
			if ((unsigned char *) goq_rp + n > btop)
			{
				rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
				mu_gvis();
				util_out_print(0,TRUE);
				break;
			}
			while (cp1 < cp2)
			{
				if (*cp1 == NEGORZRO)
				{
					cp1++;
					exp = *cp1++;
					if (exp < M11_EXP)
					{
						v.mvtype = MV_STR;
						b = v.str.addr = &k_buff;
						*b++ = '-';
						*b++ = '.';
						exp /= 2;
						exp = M11_EXP/2 - exp - 1;
						assert (exp < M11_EXP/2);

						while (*cp1 != M11_NEG && cp1 < cp2 && !mupip_error_occurred)
						{
							digit = *cp1++;
							digit /= 2;
							switch (digit)
							{
								case '0': *b++ = '9';
									break;
								case '1': *b++ = '8';
									break;
								case '2': *b++ = '7';
									break;
								case '3': *b++ = '6';
									break;
								case '4': *b++ = '5';
									break;
								case '5': *b++ = '4';
									break;
								case '6': *b++ = '3';
									break;
								case '7': *b++ = '2';
									break;
								case '8': *b++ = '1';
									break;
								case '9': *b++ = '0';
									break;
								default:
									rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,
										global_key_count);
									 mu_gvis();
									 util_out_print(0,TRUE);
							}
						}
						if (!mupip_error_occurred)
						{
							if (*cp1++ != M11_NEG)
							{	rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
								mu_gvis();
								util_out_print(0,TRUE);
								break;
							}
							if (b - (unsigned char *) v.str.addr > GTM_PREC + 2)
							{
								mupip_precision_error = TRUE;
							}
							*b++ = 'E';
							if (order = exp/10)
								*b++ = order + 48;
							*b++ = exp - order*10 + 48;
							v.str.len = b - (unsigned char *) v.str.addr;
							s2n(&v);
							if (mupip_error_occurred)
							{	rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
								mu_gvis();
								util_out_print(0,TRUE);
								break;
							}
							else
								v.mvtype = MV_NM;
						}
					}
					else if (exp == M11_ZRO)
					{
						v.mvtype = MV_NM;
						v.m[1] = 0;
					}
					else
					{	rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
						mu_gvis();
						util_out_print(0,TRUE);
						break;
					}
				}
				else if (*cp1 > NEGORZRO && *cp1 < M11_EXP)
				{
					cp1++;
					is_dec = 0;
					v.mvtype = MV_STR;
					b = v.str.addr = &k_buff;
					while (*cp1 & ODD)
					{
						*b = *cp1++ / 2;

						if (*b++ == '.')
							is_dec = 1;
					}
					*b++ = *cp1++ / 2;
					v.str.len = b - (unsigned char *) v.str.addr;
					if (cp1 > cp2)
					{
						rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
						mu_gvis();
						util_out_print(0,TRUE);
						break;
					}
					if (v.str.len > GTM_PREC + is_dec)
					{
						mupip_precision_error = TRUE;
					}
					s2n(&v);
					if (mupip_error_occurred)
					{	rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
						mu_gvis();
						util_out_print(0,TRUE);
						break;
					}
					else
						v.mvtype = MV_NM;
				}
				else
				{
					v.mvtype = MV_STR;
					b = v.str.addr = &k_buff[0];
					while (*cp1 & ODD)
					{
						*b++ = *cp1++ / 2;
					}
					*b++ = *cp1++ /2;
					v.str.len = b - (unsigned char *) v.str.addr;
					if (cp1 > cp2)
					{
						rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
						mu_gvis();
						util_out_print(0,TRUE);
						break;
					}
				}
				mval2subsc(&v,gv_currkey);
				gv_currkey->prev = 0;
				if (mupip_error_occurred)
				{
					rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
					mu_gvis();
					util_out_print(0,TRUE);
					break;
				}
				goq_key_sub++;
				goq_subsc_map[US][goq_key_sub] = gv_currkey->end;
				goq_subsc_map[THEM][goq_key_sub] = goq_currkey->end = cp1 - cp3;
			}

			if (mupip_error_occurred)
				break;

			v.mvtype = MV_STR; /* mval for datum */
			v.str.len = *dp1++;
			v.str.addr = dp1;

			if (mupip_precision_error)
			{
				op_gvdata(&exist);
				if (exist.m[1] != 0)
				{
					msg.arg_cnt = 4;
					msg.new_opts = msg.def_opts = 1;
					msg.msg_number = ERR_GOQPREC;
					msg.fp_cnt = 2;
					msg.fp[0].n = rec_count;
					msg.fp[1].n = global_key_count;
					sys$putmsg(&msg,0,0,0);
					mu_gvis();
					util_out_print(0,TRUE);
				}
				else
					op_gvput(&v);
				mupip_precision_error = FALSE;
			}
			else
				op_gvput (&v);
			if (mupip_error_occurred)
			{
				if (!mupip_DB_full)
				{
					rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
					util_out_print(0,TRUE);
				}
				break;
			}
			if (max_data_len < v.str.len)
				max_data_len = v.str.len;
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			key_count++;

			(char *) goq_rp += n;
			if (goq_rp < btop)
			{
				if (goq_rp->cmpc > goq_currkey->end)
				{
					rts_error(VARLSTCNT(4) ERR_CORRUPT,2,rec_count,global_key_count);
					break;
				}
				cp1 = &goq_rp->subsc[0];
				cp2 = &goq_currkey->base[0] + goq_rp->cmpc;
				for ( n = 0; n < goq_rp->subsc_len; n++)
					*cp2++ = *cp1++;
				n = goq_key_sub;
				for (; goq_subsc_map[THEM][n] > goq_rp->cmpc; n--)
					;
				goq_key_sub = n;
				cp1 = cp3 + goq_subsc_map[THEM][goq_key_sub];
				gv_currkey->end = goq_subsc_map[US][goq_key_sub];
				global_key_count++;
			}
			else
				break;
		}
	}

	if (mu_ctrly_occurred)
		lib$signal(ERR_LOADCTRLY);

	if (mupip_error_occurred)
		lib$signal(ERR_LOADABORT,1,rec_count);
	else
		mu_load_stat(max_data_len, max_subsc_len, key_count, rec_count, ERR_BLKCNT);

	free (goq_currkey);
	free (gv_currkey);
}
