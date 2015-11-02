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
#include "stp_parms.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "muextr.h"
#include "error.h"
#include "copy.h"
#include "collseq.h"
#include "util.h"
#include "op.h"
#include "gvsub2str.h"
#include "mupip_exit.h"
#include "file_input.h"
#include "load.h"
#include "mvalconv.h"
#include "mu_gvis.h"
#include "gtmmsg.h"
#include "gtm_utf8.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "rtnhdr.h"
#include "gv_trigger.h"
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */

GBLREF bool		mupip_DB_full;
GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mupip_error_occurred;
GBLREF spdesc		stringpool;
GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_altkey;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF int4		gv_keysize;
GBLREF gd_region	*gv_cur_region;

error_def(ERR_CORRUPT);
error_def(ERR_GVIS);
error_def(ERR_TEXT);
error_def(ERR_LDBINFMT);
error_def(ERR_LOADCTRLY);
error_def(ERR_LOADEOF);
error_def(ERR_MUNOFINISH);
error_def(ERR_COLLTYPVERSION);
error_def(ERR_COLLATIONUNDEF);
error_def(ERR_OLDBINEXTRACT);
error_def(ERR_LOADINVCHSET);

#define	BIN_PUT		0
#define BIN_BIND	1
#define ERR_COR		2
#ifdef GTM_CRYPT
#define EMPTY_GTMCRYPT_HASH16	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define EMPTY_GTMCRYPT_HASH32	EMPTY_GTMCRYPT_HASH16 EMPTY_GTMCRYPT_HASH16
#define EMPTY_GTMCRYPT_HASH	EMPTY_GTMCRYPT_HASH32 EMPTY_GTMCRYPT_HASH32
#define GC_BIN_LOAD_ERR(crypt_status)			\
{							\
	if (0 != crypt_status)				\
	{						\
		GC_GTM_PUTMSG(crypt_status, NULL);	\
		mupip_error_occurred = TRUE;		\
		if (NULL != tmp_gvkey)			\
		{					\
			free(tmp_gvkey);		\
			tmp_gvkey = NULL;		\
		}					\
		return;					\
	}						\
}
#endif

static readonly unsigned char gt_lit[] = "LOAD TOTAL";

/* starting extract file format 3, we have an extra record for each gvn, that contains the
 * collation information of the database at the time of extract. This record is transparent
 * to the user, so the semantics of the command line options, 'begin' and 'end' to MUPIP LOAD
 * will remain same. The collation header is identified in the binary extract by the fact
 * that its size is 4 bytes and no valid data record can have length 4.
 */

void		bin_call_db(int, INTPTR_T, INTPTR_T);

void bin_load(uint4 begin, uint4 end)
{
	unsigned char	*ptr, *cp1, *cp2, *btop, *gvkey_char_ptr, *tmp_ptr, *tmp_key_ptr, *c, *ctop;
	unsigned char	hdr_lvl, src_buff[MAX_KEY_SZ + 1], dest_buff[MAX_ZWR_KEY_SZ],
			cmpc_str[MAX_KEY_SZ + 1], dup_key_str[MAX_KEY_SZ + 1];
	unsigned char	*end_buff;
	unsigned short	rec_len, next_cmpc;
	int		len;
	int		current, last, length, max_blk_siz, max_key, status;
	uint4		iter, max_data_len, max_subsc_len, key_count;
	ssize_t	        rec_count, global_key_count, subsc_len,extr_std_null_coll;
	boolean_t	need_xlation, new_gvn, utf8_extract;
	rec_hdr		*rp, *next_rp;
	mval		v, tmp_mval;
	mstr		mstr_src, mstr_dest;
	collseq		*extr_collseq, *db_collseq, *save_gv_target_collseq;
	coll_hdr	extr_collhdr, db_collhdr;
	gv_key 		*tmp_gvkey = NULL;	/* null-initialize at start, will be malloced later */
	char		std_null_coll[BIN_HEADER_NUMSZ + 1];
#	ifdef GTM_CRYPT
	gtmcrypt_key_t			*encr_key_handles;
	char				*inbuf;
	int4				index;
	int				req_dec_blk_size, init_status, crypt_status;
	muext_hash_hdr_ptr_t		hash_array = NULL;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(4 == SIZEOF(coll_hdr));
	gvinit();
	v.mvtype = MV_STR;
	len = file_input_bin_get((char **)&ptr);
	hdr_lvl = EXTR_HEADER_LEVEL(ptr);
	if (!(((('4' == hdr_lvl) || ('5' == hdr_lvl)) && (BIN_HEADER_SZ == len)) || (('4' > hdr_lvl) && (V3_BIN_HEADER_SZ == len))))
	{
		rts_error(VARLSTCNT(1) ERR_LDBINFMT);
		mupip_exit(ERR_LDBINFMT);
	}
	/* expecting the level in a single character */
	assert(' ' == *(ptr + SIZEOF(BIN_HEADER_LABEL) - 3));
	if (0 != memcmp(ptr, BIN_HEADER_LABEL, SIZEOF(BIN_HEADER_LABEL) - 2) || ('2' > hdr_lvl) || *(BIN_HEADER_VERSION) < hdr_lvl)
	{	/* ignore the level check */
		rts_error(VARLSTCNT(1) ERR_LDBINFMT);
		mupip_exit(ERR_LDBINFMT);
	}
	/* check if extract was generated in UTF-8 mode */
	utf8_extract = (0 == MEMCMP_LIT(&ptr[len - BIN_HEADER_LABELSZ], UTF8_NAME)) ? TRUE : FALSE;
	if ((utf8_extract && !gtm_utf8_mode) || (!utf8_extract && gtm_utf8_mode))
	{ /* extract CHSET doesn't match $ZCHSET */
		if (utf8_extract)
			rts_error(VARLSTCNT(4) ERR_LOADINVCHSET, 2, LEN_AND_LIT("UTF-8"));
		else
			rts_error(VARLSTCNT(4) ERR_LOADINVCHSET, 2, LEN_AND_LIT("M"));
		mupip_exit(ERR_LDBINFMT);
	}
	if ('4' >= hdr_lvl)
	{	/* Binary extracts in V50000-to-V52000 (label=4) and pre-V50000 (label=3) could have a '\0' byte (NULL byte)
		 * in the middle of the string. Replace it with ' ' (space) like it would be in V52000 binary extracts and above.
		 */
		for (c = ptr, ctop = c + len; c < ctop; c++)
		{
			if ('\0' == *c)
				*c = ' ';
		}
	}
	util_out_print("Label = !AD\n", TRUE, len, ptr);
	new_gvn = FALSE;
	if (hdr_lvl > '3')
	{
		memcpy(std_null_coll, ptr + BIN_HEADER_NULLCOLLOFFSET, BIN_HEADER_NUMSZ);
		std_null_coll[BIN_HEADER_NUMSZ] = '\0';
		extr_std_null_coll = STRTOUL(std_null_coll, NULL, 10);
		if (0 != extr_std_null_coll && 1!= extr_std_null_coll)
		{
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Corrupted null collation field  in header"),
				ERR_LDBINFMT);
			mupip_exit(ERR_LDBINFMT);
		}
	} else
		extr_std_null_coll = 0;
#	ifdef GTM_CRYPT
	if ('5' <= hdr_lvl)
	{
		int	i, num_indexes;
		len = file_input_bin_get((char **)&ptr);
		hash_array = (muext_hash_hdr *)malloc(len);
		/* store hashes of all the files used during extract into muext_hash_hdr structure */
		memcpy((char *)hash_array, ptr, len);
		num_indexes = len / GTMCRYPT_HASH_LEN;
		encr_key_handles = (gtmcrypt_key_t *)malloc(SIZEOF(gtmcrypt_key_t) * num_indexes);
		INIT_PROC_ENCRYPTION(crypt_status);
		GC_BIN_LOAD_ERR(crypt_status);
		for (index = 0; index < num_indexes; index++)
		{
			if (0 == memcmp(hash_array[index].gtmcrypt_hash, EMPTY_GTMCRYPT_HASH, GTMCRYPT_HASH_LEN))
				continue;
			GTMCRYPT_GETKEY(hash_array[index].gtmcrypt_hash, encr_key_handles[index], crypt_status);
			GC_BIN_LOAD_ERR(crypt_status);
		}
	}
#	endif
	if ('2' < hdr_lvl)
	{
		len = file_input_bin_get((char **)&ptr);
		if (SIZEOF(coll_hdr) != len)
		{
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Corrupt collation header"), ERR_LDBINFMT);
			mupip_exit(ERR_LDBINFMT);
		}
		extr_collhdr = *((coll_hdr *)(ptr));
		new_gvn = TRUE;
	} else
		gtm_putmsg(VARLSTCNT(3) ERR_OLDBINEXTRACT, 1, hdr_lvl - '0');
	if (begin < 2)
		begin = 2;
	for (iter = 2; iter < begin; iter++)
	{
		if (!(len = file_input_bin_get((char **)&ptr)))
		{
			gtm_putmsg(VARLSTCNT(3) ERR_LOADEOF, 1, begin);
			util_out_print("Error reading record number: !UL\n", TRUE, iter);
			mupip_error_occurred = TRUE;
			return;
		} else if (len == SIZEOF(coll_hdr))
		{
			extr_collhdr = *((coll_hdr *)(ptr));
			assert(hdr_lvl > '2');
			iter--;
		}
	}
	assert(iter == begin);
	util_out_print("Beginning LOAD at record number: !UL\n", TRUE, begin);
	max_data_len = 0;
	max_subsc_len = 0;
	global_key_count = key_count = 0;
	rec_count = begin - 1;
	extr_collseq = db_collseq = NULL;
	need_xlation = FALSE;
	assert(NULL == tmp_gvkey);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(tmp_gvkey, DBKEYSIZE(MAX_KEY_SZ));	/* tmp_gvkey will point to malloced memory after this */
	for (; !mupip_DB_full ;)
	{
		if (++rec_count > end)
			break;
		next_cmpc = 0;
		mupip_error_occurred = FALSE;
		if (mu_ctrly_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			util_out_print("!AD:!_  Key cnt: !UL  max subsc len: !UL  max data len: !UL", TRUE,
				LEN_AND_LIT(gt_lit), key_count, max_subsc_len, max_data_len);
			util_out_print("Last LOAD record number: !UL", TRUE, key_count ? (rec_count - 1) : 0);
			mu_gvis();
			util_out_print(0, TRUE);
			mu_ctrlc_occurred = FALSE;
		}
		/* reset the stringpool for every record in order to avoid garbage collection */
		stringpool.free = stringpool.base;
		if (!(len = file_input_bin_get((char **)&ptr)) || mupip_error_occurred)
			break;
		else if (len == SIZEOF(coll_hdr))
		{
			extr_collhdr = *((coll_hdr *)(ptr));
			assert(hdr_lvl > '2');
			new_gvn = TRUE;			/* next record will contain a new gvn */
			rec_count--;	/* Decrement as this record does not count as a record for loading purposes */
			continue;
		}
		rp = (rec_hdr*)(ptr);
#		ifdef GTM_CRYPT
		if ('5' <= hdr_lvl)
		{	/* Getting index value from the extracted file. It indicates which database file this record belongs to */
			GET_LONG(index, ptr);
			if (-1 != index) /* Indicates that the record is encrypted. */
			{
				req_dec_blk_size = len - SIZEOF(int4);
				inbuf = (char *)(ptr + SIZEOF(int4));
				GTMCRYPT_DECODE_FAST(encr_key_handles[index], inbuf, req_dec_blk_size, NULL, crypt_status);
				GC_BIN_LOAD_ERR(crypt_status);
			}
			rp = (rec_hdr*)(ptr + SIZEOF(int4));
		}
#		endif
		btop = ptr + len;
		cp1 = (unsigned char*)(rp + 1);
		v.str.addr = (char*)cp1;
		while (*cp1++)
			;
		v.str.len =INTCAST((char*)cp1 - v.str.addr - 1);
		if (('2' >= hdr_lvl) || new_gvn)
		{
			if ((HASHT_GBLNAME_LEN == v.str.len) &&	(0 == memcmp(v.str.addr, HASHT_GBLNAME, HASHT_GBLNAME_LEN)))
				continue;
			bin_call_db(BIN_BIND, (INTPTR_T)gd_header, (INTPTR_T)&v.str);
			max_key = gv_cur_region->max_key_size;
			db_collhdr.act = gv_target->act;
			db_collhdr.ver = gv_target->ver;
			db_collhdr.nct = gv_target->nct;
		}
		GET_USHORT(rec_len, &rp->rsiz);
		if (rp->cmpc != 0 || v.str.len > rec_len || mupip_error_occurred)
		{
			bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
			mu_gvis();
			util_out_print(0, TRUE);
			continue;
		}
		if (new_gvn)
		{
			global_key_count = 1;
			if ((db_collhdr.act != extr_collhdr.act || db_collhdr.ver != extr_collhdr.ver
				|| db_collhdr.nct != extr_collhdr.nct
				|| gv_cur_region->std_null_coll != extr_std_null_coll))
			{
				if (extr_collhdr.act)
				{
					if (extr_collseq = ready_collseq((int)extr_collhdr.act))
					{
						if (!do_verify(extr_collseq, extr_collhdr.act, extr_collhdr.ver))
						{
							gtm_putmsg(VARLSTCNT(8) ERR_COLLTYPVERSION, 2, extr_collhdr.act,
								extr_collhdr.ver, ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
							mupip_exit(ERR_COLLTYPVERSION);
						}
					} else
					{
						gtm_putmsg(VARLSTCNT(7) ERR_COLLATIONUNDEF, 1, extr_collhdr.act,
							ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
						mupip_exit(ERR_COLLATIONUNDEF);
					}
				}
				if (db_collhdr.act)
				{
					if (db_collseq = ready_collseq((int)db_collhdr.act))
					{
						if (!do_verify(db_collseq, db_collhdr.act, db_collhdr.ver))
						{
							gtm_putmsg(VARLSTCNT(8) ERR_COLLTYPVERSION, 2, db_collhdr.act,
								db_collhdr.ver, ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
							mupip_exit(ERR_COLLTYPVERSION);
						}
					} else
					{
						gtm_putmsg(VARLSTCNT(7) ERR_COLLATIONUNDEF, 1, db_collhdr.act,
							ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
						mupip_exit(ERR_COLLATIONUNDEF);
					}
				}
				need_xlation = TRUE;
			} else
				need_xlation = FALSE;
		}
		new_gvn = FALSE;
		for (; rp < (rec_hdr*)btop; rp = (rec_hdr*)((unsigned char *)rp + rec_len))
		{
			GET_USHORT(rec_len, &rp->rsiz);
			if (rec_len + (unsigned char *)rp > btop)
			{
				bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
				mu_gvis();
				util_out_print(0, TRUE);
				break;
			}
			cp1 =  (unsigned char*)(rp + 1);
			cp2 = gv_currkey->base + rp->cmpc;
			current = 1;
			for (;;)
			{
				last = current;
				current = *cp2++ = *cp1++;
				if (0 == last && 0 == current)
					break;
				if (cp1 > (unsigned char *)rp + rec_len ||
				    cp2 > (unsigned char *)gv_currkey + gv_currkey->top)
				{
					bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
					mu_gvis();
					util_out_print(0, TRUE);
					break;
				}
			}
			if (mupip_error_occurred)
				break;
			gv_currkey->end = cp2 - gv_currkey->base - 1;
			if (need_xlation)
			{
				assert(hdr_lvl >= '3');
				assert(extr_collhdr.act || db_collhdr.act || extr_collhdr.nct || db_collhdr.nct ||
				 	extr_std_null_coll != gv_cur_region->std_null_coll);
							/* gv_currkey would have been modified/translated in the earlier put */
				memcpy(gv_currkey->base, cmpc_str, next_cmpc);
				next_rp = (rec_hdr *)((unsigned char*)rp + rec_len);
				if ((unsigned char*)next_rp < btop)
				{
					next_cmpc = next_rp->cmpc;
					assert(next_cmpc <= gv_currkey->end);
					memcpy(cmpc_str, gv_currkey->base, next_cmpc);
				} else
					next_cmpc = 0;
							/* length of the key might change (due to nct variation),
							 * so get a copy of the original key from the extract */
				memcpy(dup_key_str, gv_currkey->base, gv_currkey->end + 1);
				gvkey_char_ptr = dup_key_str;
				while (*gvkey_char_ptr++)
					;
				gv_currkey->prev = 0;
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
                                	tmp_mval.str.len = INTCAST(end_buff - dest_buff);
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
				if ( gv_cur_region->std_null_coll != extr_std_null_coll && gv_currkey->prev)
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
				bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
				mu_gvis();
				util_out_print(0, TRUE);
				continue;
			}
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			v.str.addr = (char*)cp1;
			v.str.len =INTCAST(rec_len - (cp1 - (unsigned char *)rp));
			if (max_data_len < v.str.len)
				max_data_len = v.str.len;
			bin_call_db(BIN_PUT, (INTPTR_T)&v, 0);
			if (mupip_error_occurred)
			{
				if (!mupip_DB_full)
				{
					bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
					util_out_print(0, TRUE);
				}
				break;
			}
			key_count++;
			global_key_count++;
		}
	}
	GTMCRYPT_ONLY(
		if (NULL != hash_array)
			free(hash_array);
	)
	free(tmp_gvkey);
	file_input_close();
	util_out_print("LOAD TOTAL!_!_Key Cnt: !UL  Max Subsc Len: !UL  Max Data Len: !UL", TRUE, key_count, max_subsc_len,
			max_data_len);
	util_out_print("Last LOAD record number: !UL\n", TRUE, key_count ? (rec_count - 1) : 0);
	if (mu_ctrly_occurred)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_LOADCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
}

void bin_call_db(int routine, INTPTR_T parm1, INTPTR_T parm2)
{	/* In order to duplicate the VMS functionality, which is to trap all errors in mupip_load_ch and
	 * continue in bin_load after they occur, it is necessary to call these routines from a
	 * subroutine due to the limitations of condition handlers and unwinding on UNIX
	 */

	ESTABLISH(mupip_load_ch);
	switch(routine)
	{
		case BIN_PUT:
			op_gvput((mval *)parm1);
			break;
		case BIN_BIND:
			GV_BIND_NAME_AND_ROOT_SEARCH((gd_addr *)parm1, (mstr *)parm2);
			break;
		case ERR_COR:
			rts_error(VARLSTCNT(4) ERR_CORRUPT, 2, parm1, parm2);
			break;
	}
	REVERT;
}
