/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_ctype.h"
#include "gtm_stdlib.h"

#include "stringpool.h"
#include <rms.h>
#include "iormdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "msg.h"
#include "muextr.h"
#include "outofband.h"
#include "collseq.h"
#include "copy.h"
#include "util.h"
#include "op.h"
#include "gvsub2str.h"
#include "error.h"
#include "mu_load_stat.h"
#include "load.h"
#include "mvalconv.h"
#include "mu_gvis.h"
#include "gtmmsg.h"

#define 	CR 13
#define 	LF 10
#define		LCL_BUF_SIZE 512
#define 	FILLFACTOR_EXPONENT 10
#define		V3_STDNULLCOLL	"00000"	/* V3 denotes binary extract header version number (not GT.M version number) */

GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mupip_DB_full;
GBLREF bool		mupip_error_occurred;
GBLREF spdesc 		stringpool;
GBLREF gv_key		*gv_altkey;
GBLREF gv_key		*gv_currkey;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_addr		*gd_header;
GBLREF int4		gv_keysize;
GBLREF gv_namehead	*gv_target;

error_def(ERR_BEGINST);
error_def(ERR_BINHDR);
error_def(ERR_BLKCNT);
error_def(ERR_COLLTYPVERSION);
error_def(ERR_COLLATIONUNDEF);
error_def(ERR_CORRUPT);
error_def(ERR_GVIS);
error_def(ERR_LDBINFMT);
error_def(ERR_LOADABORT);
error_def(ERR_LOADCTRLY);
error_def(ERR_LOADEOF);
error_def(ERR_MUNOACTION);
error_def(ERR_OLDBINEXTRACT);
error_def(ERR_PREMATEOF);
error_def(ERR_TEXT);

/***********************************************************************************************/
/*					Binary Format                                          */
/***********************************************************************************************/

/* starting extract file format 3, we have an extra record for each gvn, that contains the
 * collation information of the database at the time of extract. This record is transparent
 * to the user, so the semantics of the command line options, 'begin' and 'end' to MUPIP LOAD
 * will remain same. The collation header is identified in the binary extract by the fact
 * that its size is 4 bytes and no valid data record can have length 4.
 */

void bin_load(uint4 begin, uint4 end, struct RAB *inrab, struct FAB *infab)
{

	boolean_t	need_xlation, new_gvn;
	char 		*buff, std_null_coll[BIN_HEADER_NUMSZ + 1];
	coll_hdr	db_collhdr, extr_collhdr;
	collseq		*db_collseq, *extr_collseq, *save_gv_target_collseq;
	gv_key 		*tmp_gvkey = NULL;	/* null-initialize at start, will be malloced later */
	int 		current, last, len, max_blk_siz, max_key, other_rsz, status, subsc_len;
	msgtype		msg;
	mval		tmp_mval, v;
	rec_hdr		*next_rp, *rp;
	uint4 		extr_std_null_coll, global_key_count, key_count, max_data_len, max_subsc_len, rec_count;
	unsigned char	*btop, *cp1, *cp2, *end_buff, *gvkey_char_ptr, hdr_lvl ,*tmp_ptr, *tmp_key_ptr,
			cmpc_str[MAX_KEY_SZ + 1], dest_buff[MAX_ZWR_KEY_SZ], dup_key_str[MAX_KEY_SZ + 1], src_buff[MAX_KEY_SZ + 1];
	unsigned short	next_cmpc, rec_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(4 == SIZEOF(coll_hdr));
	inrab->rab$l_ubf = malloc(LCL_BUF_SIZE);
	inrab->rab$w_usz = LCL_BUF_SIZE - 1;
	max_data_len = max_subsc_len = key_count = 0;
	rec_count = 1;
	status = sys$get(inrab);
	if (RMS$_EOF == status)
		rts_error(VARLSTCNT(1) ERR_PREMATEOF);
	if (!(status & 1))
		rts_error(VARLSTCNT(1) status);
	len = inrab->rab$w_rsz;
	buff = inrab->rab$l_rbf;
	while ((0 < len) && ((LF == buff[len - 1]) || (CR == buff[len - 1])))
		len--;
	/* expect the level can be represented in a single character */
        assert(' ' == *(buff + SIZEOF(BIN_HEADER_LABEL) - 3));
	hdr_lvl = EXTR_HEADER_LEVEL(buff);
        if (0 != memcmp(buff, BIN_HEADER_LABEL, SIZEOF(BIN_HEADER_LABEL) - 2) || '2' > hdr_lvl
	    || *(BIN_HEADER_VERSION) < hdr_lvl)
	{				/* ignore the level check */
		rts_error(VARLSTCNT(1) ERR_LDBINFMT);
		return;
	}
	if ('3' < hdr_lvl)
	{
		memcpy(std_null_coll, buff + BIN_HEADER_NULLCOLLOFFSET, BIN_HEADER_NUMSZ);
		std_null_coll[BIN_HEADER_NUMSZ] = '\0';
		extr_std_null_coll = STRTOUL(std_null_coll, NULL, 10);
		if (0 != extr_std_null_coll && 1!= extr_std_null_coll)
		{
                	rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Corrupted null collation field in header"),
				ERR_LDBINFMT);
			return;
		}
	} else
	{
		memcpy(std_null_coll, V3_STDNULLCOLL, BIN_HEADER_NUMSZ);
		assert(BIN_HEADER_NUMSZ == STR_LIT_LEN(V3_STDNULLCOLL));
		std_null_coll[BIN_HEADER_NUMSZ] = '\0';
		extr_std_null_coll = 0;
	}
	msg.arg_cnt = 18;
	msg.new_opts = msg.def_opts = 1;
	msg.msg_number = ERR_BINHDR;
	msg.fp_cnt = 16;
	msg.fp[0].n = SIZEOF(BIN_HEADER_LABEL) - 1;
	msg.fp[1].cp = buff;
	msg.fp[2].n = SIZEOF("YEARMMDD") - 1;
	msg.fp[3].cp = buff + SIZEOF(BIN_HEADER_LABEL) - 1;
	msg.fp[4].n = SIZEOF(BIN_HEADER_DATEFMT) - SIZEOF("YEARMMDD");
	msg.fp[5].cp = buff + SIZEOF("YEARMMDD") + SIZEOF(BIN_HEADER_LABEL) - 2;
	msg.fp[6].n = BIN_HEADER_NUMSZ;
	msg.fp[7].cp = buff + BIN_HEADER_BLKOFFSET;
	msg.fp[8].n = BIN_HEADER_NUMSZ;
	msg.fp[9].cp = buff + BIN_HEADER_RECOFFSET;
	msg.fp[10].n = BIN_HEADER_NUMSZ;
	msg.fp[11].cp = buff + BIN_HEADER_KEYOFFSET;
	msg.fp[12].n = BIN_HEADER_NUMSZ;
	msg.fp[13].cp = &std_null_coll[0];
	if (hdr_lvl > '3')
	{
		msg.fp[14].n = BIN_HEADER_SZ - (BIN_HEADER_NULLCOLLOFFSET + BIN_HEADER_NUMSZ);
		msg.fp[15].cp = buff + BIN_HEADER_NULLCOLLOFFSET + BIN_HEADER_NUMSZ;
	} else
	{
		msg.fp[14].n = V3_BIN_HEADER_SZ - (BIN_HEADER_KEYOFFSET + BIN_HEADER_NUMSZ);
		msg.fp[15].cp = buff + BIN_HEADER_KEYOFFSET + BIN_HEADER_NUMSZ;
	}
	sys$putmsg(&msg, 0, 0, 0);
	v.mvtype = MV_STR;
	v.str.len = BIN_HEADER_NUMSZ;
	v.str.addr = buff + BIN_HEADER_BLKOFFSET;
	s2n(&v);
	stringpool.free = stringpool.base;
	max_blk_siz = MV_FORCE_INTD(&v);
	assert(max_blk_siz > LCL_BUF_SIZE - 1);
	infab->fab$w_mrs = max_blk_siz;
	/* Note the buffer size below is the same as the blocksize but the extract data will not contain the block header.
	 * But rather than reduce the buffer by that amount we just leave it (somewhat) larger. Reason is beginning with V5
	 * we also accept V4 extracts which had a smaller block header so the real size is indeterminate and since the
	 * difference is only a few bytes, we leave the buffer size at the full blocksize. SE 4/2005
	 */
	inrab->rab$w_usz = max_blk_siz;
	free(inrab->rab$l_ubf);
	inrab->rab$l_ubf = malloc(inrab->rab$w_usz);
	v.mvtype = MV_STR;
	rec_count++;
	new_gvn = FALSE;
	if ('2' < hdr_lvl)
	{
		status = sys$get(inrab);
		if (RMS$_EOF == status)
			rts_error(VARLSTCNT(1) ERR_PREMATEOF);
		if (!(status & 1))
			rts_error(VARLSTCNT(1) status);
		if (SIZEOF(coll_hdr) != inrab->rab$w_rsz)
                {
                        rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Corrupt collation header"), ERR_LDBINFMT);
			return;
                }
		extr_collhdr = *((coll_hdr *)(inrab->rab$l_rbf));
		new_gvn = TRUE;
	} else
		gtm_putmsg(VARLSTCNT(3) ERR_OLDBINEXTRACT, 1, hdr_lvl - '0');
	if (begin < 2)
		begin = 2;
	for ( ; rec_count < begin; )
	{
		status = sys$get(inrab);
		if (RMS$_EOF == status)
		{
			sys$close(infab);
			gtm_putmsg(VARLSTCNT(3) ERR_LOADEOF, 1, begin);
			mupip_exit(ERR_MUNOACTION);
		}
		if (RMS$_NORMAL != status)
			rts_error(VARLSTCNT(1) status);
		if (SIZEOF(coll_hdr) == inrab->rab$w_rsz)
		{
			assert(hdr_lvl > '2');
			continue;
		}
		rec_count++;
	}
	msg.msg_number = ERR_BEGINST;
	msg.arg_cnt = 3;
	msg.fp_cnt = 1;
	msg.fp[0].n = rec_count;
	sys$putmsg(&msg, 0, 0, 0);
	ESTABLISH(mupip_load_ch);
	extr_collseq = db_collseq = NULL;
	need_xlation = FALSE;
	rec_count = begin - 1;
	other_rsz = 0;
	assert(NULL == tmp_gvkey);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(tmp_gvkey, DBKEYSIZE(MAX_KEY_SZ));	/* tmp_gvkey will point to malloced memory after this */
	for ( ; !mupip_DB_full; )
	{
		if (++rec_count > end)
			break;
		next_cmpc = 0;
		mupip_error_occurred = FALSE;
		if (mu_ctrly_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			mu_load_stat(max_data_len, max_subsc_len, key_count, key_count ? (rec_count - 1) : 0, ERR_BLKCNT);
			mu_gvis();
			util_out_print(0, TRUE);
		}
		/* reset the stringpool for every record in order to avoid garbage collection */
		stringpool.free = stringpool.base;
		if (other_rsz)
		{	/* read an extra block when it wasn't a tail, but rather a new record */
			inrab->rab$w_rsz = other_rsz;
			rp = inrab->rab$l_ubf + MAX_BIN_WRT;
			btop = rp + other_rsz;
			other_rsz = 0;
		} else
		{
			if (RMS$_EOF == (status = sys$get(inrab)))
				break;
			if (RMS$_NORMAL != status)
			{
				lib$signal(status);
				mupip_error_occurred = TRUE;
				break;
			}
			assert((max_blk_siz >= inrab->rab$w_rsz) && (MAX_BIN_WRT >= inrab->rab$w_rsz));
			rp = inrab->rab$l_rbf;
			btop = inrab->rab$l_rbf + inrab->rab$w_rsz;
			if (MAX_BIN_WRT == inrab->rab$w_rsz)
			{	/* Most likely there's more, so do another read */
				inrab->rab$l_ubf += MAX_BIN_WRT;
				status = sys$get(inrab);
				if ((RMS$_NORMAL != status) && ((end != rec_count) || (RMS$_EOF != status)))
				{
					lib$signal(status);
					mupip_error_occurred = TRUE;
					break;
				}
				other_rsz = inrab->rab$w_rsz;
				inrab->rab$l_ubf = rp;
			}
		}
		if (SIZEOF(coll_hdr) == inrab->rab$w_rsz)
		{
			extr_collhdr = *((coll_hdr *)(inrab->rab$l_rbf));
			assert(hdr_lvl > '2');
			new_gvn = TRUE;                 /* next record will contain a new gvn */
			rec_count--;	/* Decrement as this record does not count as a record for loading purposes */
			continue;
		}
		cp1 = rp + 1;
		v.str.addr = cp1;
		while (*cp1++)
			;
		v.str.len = cp1 - (unsigned char *)v.str.addr - 1;
		if (hdr_lvl <= '2' || new_gvn)
		{
			GV_BIND_NAME_AND_ROOT_SEARCH(gd_header, &v.str);
			max_key = gv_cur_region->max_key_size;
			db_collhdr.act = gv_target->act;
			db_collhdr.ver = gv_target->ver;
			db_collhdr.nct = gv_target->nct;
		}
		if ((0 != rp->cmpc) || (v.str.len > rp->rsiz) || mupip_error_occurred)
		{
			rts_error(VARLSTCNT(4) ERR_CORRUPT, 2, rec_count, global_key_count);
			mu_gvis();
			util_out_print(0, TRUE);
			continue;
		}
		if (new_gvn)
		{
			global_key_count = 1;
			if ((db_collhdr.act != extr_collhdr.act || db_collhdr.ver != extr_collhdr.ver ||
				db_collhdr.nct != extr_collhdr.nct || extr_std_null_coll != gv_cur_region->std_null_coll))
				/* do we need to bother about 'ver' change ??? */
			{
				if (extr_collhdr.act)
				{
					if (extr_collseq = ready_collseq((int)extr_collhdr.act))
					{
						if (!do_verify(extr_collseq, extr_collhdr.act, extr_collhdr.ver))
						{
							rts_error(VARLSTCNT(8) ERR_COLLTYPVERSION, 2,
								extr_collhdr.act, extr_collhdr.ver,
								ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
						}
					} else
					{
						rts_error(VARLSTCNT(7) ERR_COLLATIONUNDEF, 1, extr_collhdr.act,
							ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
					}
				}
				if (db_collhdr.act)
				{
					if (db_collseq = ready_collseq((int)db_collhdr.act))
					{
						if (!do_verify(db_collseq, db_collhdr.act, db_collhdr.ver))
						{
							rts_error(VARLSTCNT(8) ERR_COLLTYPVERSION, 2, db_collhdr.act,
								db_collhdr.ver,
								ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
						}
					} else
					{
						rts_error(VARLSTCNT(7) ERR_COLLATIONUNDEF, 1, db_collhdr.act,
							ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
					}
				}
				need_xlation = TRUE;
			} else
				need_xlation = FALSE;
		}
		new_gvn = FALSE;
		GET_USHORT(rec_len, &rp->rsiz);
		for (; rp < btop; rp = (unsigned char *)rp + rec_len)
		{
			GET_USHORT(rec_len, &rp->rsiz);
			if ((rec_len + (unsigned char *)rp > btop) && (other_rsz))
			{	/* if there was a second read try to use it */
				btop += other_rsz;
				other_rsz = 0;
			}
			if (rec_len + (unsigned char *)rp > btop)
			{
				rts_error(VARLSTCNT(4) ERR_CORRUPT, 2, rec_count, global_key_count);
				mu_gvis();
				util_out_print(0, TRUE);
				break;
			}
			cp1 = rp + 1;
			cp2 = &gv_currkey->base + rp->cmpc;
			current = 1;
			for (;;)
			{
				last = current;
				current = *cp2++ = *cp1++;
				if ((0 == last) && (0 == current))
					break;
				if ((cp1 > ((unsigned char *)rp + rec_len)) ||
				    (cp2 > ((unsigned char *)gv_currkey + gv_currkey->top)))
				{
					rts_error(VARLSTCNT(4) ERR_CORRUPT, 2, rec_count, global_key_count);
					mu_gvis();
					util_out_print(0, TRUE);
					break;
				}
			}
			if (mupip_error_occurred)
				break;
			gv_currkey->end = cp2 - (unsigned char *)&gv_currkey->base - 1;
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			if (need_xlation)
			{				/* gv_currkey would have been modified/translated in the earlier put */
				memcpy(gv_currkey->base, cmpc_str, next_cmpc);
				next_rp = (rec_hdr *)((unsigned char*)rp + rec_len);
				if ((unsigned char*)next_rp < btop)
				{
					next_cmpc = next_rp->cmpc;
					assert(next_cmpc <= gv_currkey->end);
					memcpy(cmpc_str, gv_currkey->base, next_cmpc);
				} else
					next_cmpc = 0;
				assert(hdr_lvl >= '3');
				assert(extr_collhdr.act || db_collhdr.act || extr_collhdr.nct || db_collhdr.nct ||
				 	extr_std_null_coll != gv_cur_region->std_null_coll);
							/* the length of the key might change (due to nct variation),
							 * so get a copy of the original key from the extract */
				memcpy(dup_key_str, gv_currkey->base, gv_currkey->end + 1);
				gvkey_char_ptr = dup_key_str;
				while (*gvkey_char_ptr++)
					;
				gv_currkey->end = gvkey_char_ptr - dup_key_str;
				assert(gv_keysize <= tmp_gvkey->top);
				while (*gvkey_char_ptr)
				{
						/* get next subscript (in GT.M internal subsc format) */
					subsc_len = 0;
					tmp_ptr = src_buff;
					while (*gvkey_char_ptr)
						*tmp_ptr++ = *gvkey_char_ptr++;
					subsc_len = tmp_ptr - src_buff;
					src_buff[subsc_len] = '\0';
					if (extr_collseq)
					{
						/* undo the extract time collation */
						TREF(transform) = TRUE;
						save_gv_target_collseq = gv_target->collseq;
						gv_target->collseq = extr_collseq;
					} else
						TREF(transform) = FALSE;
						/* convert the subscript to string format */
					end_buff = gvsub2str(src_buff, dest_buff, FALSE);
						/* transform the string to the current subsc format */
					TREF(transform) = TRUE;
					tmp_mval.mvtype = MV_STR;
					tmp_mval.str.addr = (char *)dest_buff;
					tmp_mval.str.len = end_buff - dest_buff;
					tmp_gvkey->prev = 0;
					tmp_gvkey->end = 0;
					if (extr_collseq)
						gv_target->collseq = save_gv_target_collseq;
					mval2subsc(&tmp_mval, tmp_gvkey);
					/* we now have the correctly transformed subscript */
					tmp_key_ptr = gv_currkey->base + gv_currkey->end;
					memcpy(tmp_key_ptr, tmp_gvkey->base, tmp_gvkey->end + 1);
					gv_currkey->prev = gv_currkey->end;
					gv_currkey->end += tmp_gvkey->end;
					gvkey_char_ptr++;
				}
				if ((gv_cur_region->std_null_coll != extr_std_null_coll) && gv_currkey->prev)
				{
					if (extr_std_null_coll == 0)
					{
						GTM2STDNULLCOLL(gv_currkey->base, gv_currkey->end);
					} else
					{
						STD2GTMNULLCOLL(gv_currkey->base, gv_currkey->end);
					}
				}
			}
			if (gv_currkey->end >= max_key)
			{
				rts_error(VARLSTCNT(4) ERR_CORRUPT, 2, rec_count, global_key_count);
				mu_gvis();
				util_out_print(0, TRUE);
				continue;
			}
			v.str.addr = cp1;
			v.str.len = rec_len - (cp1 - (unsigned char *)rp);
			if (max_data_len < v.str.len)
				max_data_len = v.str.len;
			op_gvput(&v);
			if (mupip_error_occurred)
			{
				if (!mupip_DB_full)
				{
					rts_error(VARLSTCNT(4) ERR_CORRUPT, 2, rec_count, global_key_count);
					util_out_print(0, TRUE);
				}
				break;
			}
			key_count++;
			global_key_count++;
		}
	}
	free(tmp_gvkey);
	lib$revert();
	status = sys$close(infab);
	if (RMS$_NORMAL != status)
	{
		lib$signal(status);
		mupip_error_occurred = TRUE;
	}
	mu_load_stat(max_data_len, max_subsc_len, key_count, key_count ? (rec_count - 1) : 0, ERR_BLKCNT);
	if (mupip_error_occurred)
		lib$signal(ERR_LOADABORT, 1, rec_count - 1);
	if (mu_ctrly_occurred)
		lib$signal(ERR_LOADCTRLY);
	free(inrab->rab$l_ubf);
}
