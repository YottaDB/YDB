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
#include "io.h"
#include "gtmcrypt.h"
#endif
#include <rtnhdr.h>
#include "gv_trigger.h"
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */
#include "format_targ_key.h"
#include "zshow.h"

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
GBLREF sgmnt_addrs	*cs_addrs;
#ifdef GTM_CRYPT
GBLREF io_pair		io_curr_device;
#endif

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
error_def(ERR_LDSPANGLOINCMP);

#define	BIN_PUT		0
#define BIN_BIND	1
#define ERR_COR		2
#define BIN_KILL	3

#ifdef GTM_CRYPT
# define EMPTY_GTMCRYPT_HASH16	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
# define EMPTY_GTMCRYPT_HASH32	EMPTY_GTMCRYPT_HASH16 EMPTY_GTMCRYPT_HASH16
# define EMPTY_GTMCRYPT_HASH	EMPTY_GTMCRYPT_HASH32 EMPTY_GTMCRYPT_HASH32

# define GC_BIN_LOAD_ERR(GTMCRYPT_ERRNO)											\
{																\
	io_log_name		*io_log;											\
																\
	if (0 != GTMCRYPT_ERRNO)												\
	{															\
		io_log = io_curr_device.in->name;										\
		GTMCRYPT_REPORT_ERROR(GTMCRYPT_ERRNO, gtm_putmsg, io_log->len, io_log->dollar_io);				\
		mupip_error_occurred = TRUE;											\
		if (NULL != tmp_gvkey)												\
		{														\
			free(tmp_gvkey);											\
			tmp_gvkey = NULL;											\
		}														\
		return;														\
	}															\
}
#endif

#define	DEFAULT_SN_HOLD_BUFF_SIZE MAX_IO_BLOCK_SIZE

#define KILL_INCMP_SN_IF_NEEDED													\
{																\
	if (!sn_incmp_gbl_already_killed)											\
	{															\
		COPY_KEY(sn_savekey,  gv_currkey);										\
		COPY_KEY(gv_currkey, sn_gvkey);											\
		bin_call_db(BIN_KILL, 0, 0);											\
		COPY_KEY(gv_currkey, sn_savekey);										\
		sn_incmp_gbl_already_killed = TRUE;										\
	}															\
}

#define DISPLAY_INCMP_SN_MSG													\
{																\
	file_offset = file_offset_base + ((unsigned char *)rp - ptr_base);							\
	if (file_offset != last_sn_error_offset)										\
	{															\
		last_sn_error_offset = file_offset;										\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LDSPANGLOINCMP);							\
		util_out_print("!_!_at File offset : [0x!XL]", TRUE, file_offset);						\
		if (sn_gvkey->end && expected_sn_chunk_number)									\
		{														\
			sn_key_str_end = format_targ_key(&sn_key_str[0], MAX_ZWR_KEY_SZ, sn_gvkey, TRUE);			\
			util_out_print("!_!_Expected Spanning Global variable : !AD", TRUE,					\
					sn_key_str_end - &sn_key_str[0], sn_key_str);						\
		}														\
		sn_key_str_end = format_targ_key(&sn_key_str[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE);				\
		util_out_print("!_!_Global variable from record: !AD", TRUE,							\
				sn_key_str_end - &sn_key_str[0], sn_key_str);							\
	}															\
}

#define DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK										\
{																\
	util_out_print("!_!_File offset : [0x!XL]", TRUE, file_offset_base + ((unsigned char *)rp - ptr_base));			\
	util_out_print("!_!_Rest of Block :", TRUE);										\
	zwr_out_print((char *)rp, btop - (unsigned char *)rp);									\
	util_out_print(0, TRUE);												\
}

#define DISPLAY_CURRKEY														\
{																\
	sn_key_str_end = format_targ_key(&sn_key_str[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE);					\
	util_out_print("!_!_Key: !AD", TRUE, sn_key_str_end - &sn_key_str[0], sn_key_str);					\
}

#define DISPLAY_VALUE(STR)													\
{																\
	util_out_print(STR, TRUE);												\
	zwr_out_print(v.str.addr, v.str.len);											\
	util_out_print(0, TRUE);												\
}

#define DISPLAY_PARTIAL_SN_HOLD_BUFF												\
{																\
	util_out_print("!_!_Partial Value :", TRUE);										\
	zwr_out_print(sn_hold_buff, sn_hold_buff_pos);										\
	util_out_print(0, TRUE);												\
}

/* starting extract file format 3, we have an extra record for each gvn, that contains the
 * collation information of the database at the time of extract. This record is transparent
 * to the user, so the semantics of the command line options, 'begin' and 'end' to MUPIP LOAD
 * will remain same. The collation header is identified in the binary extract by the fact
 * that its size is 4 bytes and no valid data record can have length 4.
 */

void		bin_call_db(int, INTPTR_T, INTPTR_T);
void 		zwr_out_print(char * buff, int len);

#define ZWR_BASE_STRIDE 1024
#define UOP_BASE_STRIDE 1024

void zwr_out_print(char * buff, int bufflen)
{
	char zwrbuff[ZWR_BASE_STRIDE * MAX_ZWR_EXP_RATIO], savechar;
	int zwrlen;
	int buffpos, left, stride;
	int uopbuffpos, uopleft, uopstride;

	buffpos = 0;
	while (left = bufflen - buffpos)
	{
		if (buffpos)
			FPRINTF(stderr,"_");
		stride = (left > ZWR_BASE_STRIDE) ? ZWR_BASE_STRIDE : left;
		format2zwr((sm_uc_ptr_t)(buff + buffpos), stride, (unsigned char *) zwrbuff, &zwrlen);
		uopbuffpos = 0;
		while (uopleft = zwrlen - uopbuffpos)
		{
			uopstride = (uopleft > UOP_BASE_STRIDE) ? UOP_BASE_STRIDE : uopleft;
			savechar = *(zwrbuff + uopstride);
			*(zwrbuff + uopstride) = '\0';
			FPRINTF(stderr,"%s", zwrbuff + uopbuffpos);
			*(zwrbuff + uopstride) = savechar;
			uopbuffpos += uopstride;
		}
		buffpos += stride;
	}
	FPRINTF(stderr,"\n");
}

void bin_load(uint4 begin, uint4 end)
{
	unsigned char		*ptr, *cp1, *cp2, *btop, *gvkey_char_ptr, *tmp_ptr, *tmp_key_ptr, *c, *ctop, *ptr_base;
	unsigned char		hdr_lvl, src_buff[MAX_KEY_SZ + 1], dest_buff[MAX_ZWR_KEY_SZ],
				cmpc_str[MAX_KEY_SZ + 1], dup_key_str[MAX_KEY_SZ + 1], sn_key_str[MAX_KEY_SZ + 1], *sn_key_str_end;
	unsigned char		*end_buff;
	unsigned short		rec_len, next_cmpc, numsubs;
	int			len;
	int			current, last, length, max_blk_siz, max_key, status;
	int			tmp_cmpc, sn_chunk_number, expected_sn_chunk_number = 0, sn_hold_buff_pos, sn_hold_buff_size;
	uint4			iter, max_data_len, max_subsc_len, key_count, gblsize;
	ssize_t			rec_count, global_key_count, subsc_len,extr_std_null_coll, last_sn_error_offset=0,
					file_offset_base=0, file_offset=0;
	boolean_t		need_xlation, new_gvn, utf8_extract;
	boolean_t		is_hidden_subscript, ok_to_put = TRUE, putting_a_sn = FALSE, sn_incmp_gbl_already_killed = FALSE;
	rec_hdr			*rp, *next_rp;
	mval			v, tmp_mval;
	mstr			mstr_src, mstr_dest;
	collseq			*extr_collseq, *db_collseq, *save_gv_target_collseq;
	coll_hdr		extr_collhdr, db_collhdr;
	gv_key			*tmp_gvkey = NULL;	/* null-initialize at start, will be malloced later */
	gv_key			*sn_gvkey = NULL; /* null-initialize at start, will be malloced later */
	gv_key			*sn_savekey = NULL; /* null-initialize at start, will be malloced later */
	char			std_null_coll[BIN_HEADER_NUMSZ + 1], *sn_hold_buff = NULL, *sn_hold_buff_temp = NULL;
#	ifdef GTM_CRYPT
	gtmcrypt_key_t		*encr_key_handles;
	char			*inbuf;
	int4			index;
	int			in_len, gtmcrypt_errno;
	muext_hash_hdr_ptr_t	hash_array = NULL;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(4 == SIZEOF(coll_hdr));
	gvinit();
	v.mvtype = MV_STR;
	len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base);
	hdr_lvl = EXTR_HEADER_LEVEL(ptr);
	if (!(((('4' == hdr_lvl) || ('5' == hdr_lvl)) && (V5_BIN_HEADER_SZ == len)) ||
			(('6' == hdr_lvl) && (BIN_HEADER_SZ == len)) ||
			(('7' == hdr_lvl) && (BIN_HEADER_SZ == len)) ||
			(('4' > hdr_lvl) && (V3_BIN_HEADER_SZ == len))))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LDBINFMT);
		mupip_exit(ERR_LDBINFMT);
	}
	/* expecting the level in a single character */
	assert(' ' == *(ptr + SIZEOF(BIN_HEADER_LABEL) - 3));
	if (0 != memcmp(ptr, BIN_HEADER_LABEL, SIZEOF(BIN_HEADER_LABEL) - 2) || ('2' > hdr_lvl) ||
			*(BIN_HEADER_VERSION_ENCR) < hdr_lvl)
	{	/* ignore the level check */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LDBINFMT);
		mupip_exit(ERR_LDBINFMT);
	}
	/* check if extract was generated in UTF-8 mode */
	utf8_extract = (0 == MEMCMP_LIT(&ptr[len - BIN_HEADER_LABELSZ], UTF8_NAME)) ? TRUE : FALSE;
	if ((utf8_extract && !gtm_utf8_mode) || (!utf8_extract && gtm_utf8_mode))
	{ /* extract CHSET doesn't match $ZCHSET */
		if (utf8_extract)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LOADINVCHSET, 2, LEN_AND_LIT("UTF-8"));
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LOADINVCHSET, 2, LEN_AND_LIT("M"));
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
		if (hdr_lvl > '5')
		{
			memcpy(std_null_coll, ptr + BIN_HEADER_NULLCOLLOFFSET, BIN_HEADER_NUMSZ);
			std_null_coll[BIN_HEADER_NUMSZ] = '\0';
		}
		else
		{
			memcpy(std_null_coll, ptr + V5_BIN_HEADER_NULLCOLLOFFSET, V5_BIN_HEADER_NUMSZ);
			std_null_coll[V5_BIN_HEADER_NUMSZ] = '\0';
		}
		extr_std_null_coll = STRTOUL(std_null_coll, NULL, 10);
		if (0 != extr_std_null_coll && 1!= extr_std_null_coll)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2,
				      RTS_ERROR_TEXT("Corrupted null collation field in header"), ERR_LDBINFMT);
			mupip_exit(ERR_LDBINFMT);
		}
	} else
		extr_std_null_coll = 0;
#	ifdef GTM_CRYPT
	if ('7' <= hdr_lvl)
	{
		int	i, num_indexes;
		len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base);
		hash_array = (muext_hash_hdr *)malloc(len);
		/* store hashes of all the files used during extract into muext_hash_hdr structure */
		memcpy((char *)hash_array, ptr, len);
		num_indexes = len / GTMCRYPT_HASH_LEN;
		encr_key_handles = (gtmcrypt_key_t *)malloc(SIZEOF(gtmcrypt_key_t) * num_indexes);
		INIT_PROC_ENCRYPTION(NULL, gtmcrypt_errno);
		GC_BIN_LOAD_ERR(gtmcrypt_errno);
		for (index = 0; index < num_indexes; index++)
		{
			if (0 == memcmp(hash_array[index].gtmcrypt_hash, EMPTY_GTMCRYPT_HASH, GTMCRYPT_HASH_LEN))
				continue;
			GTMCRYPT_GETKEY(NULL, hash_array[index].gtmcrypt_hash, encr_key_handles[index], gtmcrypt_errno);
			GC_BIN_LOAD_ERR(gtmcrypt_errno);
		}
	}
#	endif
	if ('2' < hdr_lvl)
	{
		len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base);
		if (SIZEOF(coll_hdr) != len)
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Corrupt collation header"),
				      ERR_LDBINFMT);
			mupip_exit(ERR_LDBINFMT);
		}
		extr_collhdr = *((coll_hdr *)(ptr));
		new_gvn = TRUE;
	} else
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_OLDBINEXTRACT, 1, hdr_lvl - '0');
	if (begin < 2)
		begin = 2;
	for (iter = 2; iter < begin; iter++)
	{
		if (!(len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base)))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_LOADEOF, 1, begin);
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
	assert(NULL == sn_gvkey);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(sn_gvkey, DBKEYSIZE(MAX_KEY_SZ));	/* sn_gvkey will point to malloced memory after this */
	assert(NULL == sn_savekey);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(sn_savekey, DBKEYSIZE(MAX_KEY_SZ));	/* sn_gvkey will point to malloced memory after this */
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
				LEN_AND_LIT("LOAD TOTAL"), key_count, max_subsc_len, max_data_len);
			util_out_print("Last LOAD record number: !UL", TRUE, key_count ? (rec_count - 1) : 0);
			mu_gvis();
			util_out_print(0, TRUE);
			mu_ctrlc_occurred = FALSE;
		}
		if (!(len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base)) || mupip_error_occurred)
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
		if ('7' <= hdr_lvl)
		{	/* Getting index value from the extracted file. It indicates which database file this record belongs to */
			GET_LONG(index, ptr);
			if (-1 != index) /* Indicates that the record is encrypted. */
			{
				in_len = len - SIZEOF(int4);
				inbuf = (char *)(ptr + SIZEOF(int4));
				GTMCRYPT_DECRYPT(NULL, encr_key_handles[index], inbuf, in_len, NULL, gtmcrypt_errno);
				GC_BIN_LOAD_ERR(gtmcrypt_errno);
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
		if (EVAL_CMPC(rp) != 0 || v.str.len > rec_len || mupip_error_occurred)
		{
			bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
			mu_gvis();
			DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
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
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_COLLTYPVERSION, 2,
								       extr_collhdr.act, extr_collhdr.ver, ERR_GVIS, 2,
								       gv_altkey->end - 1, gv_altkey->base);
							mupip_exit(ERR_COLLTYPVERSION);
						}
					} else
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_COLLATIONUNDEF, 1,
							       extr_collhdr.act, ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
						mupip_exit(ERR_COLLATIONUNDEF);
					}
				}
				if (db_collhdr.act)
				{
					if (db_collseq = ready_collseq((int)db_collhdr.act))
					{
						if (!do_verify(db_collseq, db_collhdr.act, db_collhdr.ver))
						{
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_COLLTYPVERSION, 2,
								       db_collhdr.act, db_collhdr.ver, ERR_GVIS, 2,
								       gv_altkey->end - 1, gv_altkey->base);
							mupip_exit(ERR_COLLTYPVERSION);
						}
					} else
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_COLLATIONUNDEF, 1, db_collhdr.act,
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
				DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
				break;
			}
			cp1 =  (unsigned char*)(rp + 1);
			cp2 = gv_currkey->base + EVAL_CMPC(rp);
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
					gv_currkey->end = cp2 - gv_currkey->base - 1;
					gv_currkey->base[gv_currkey->end] = 0;
					gv_currkey->base[gv_currkey->end - 1] = 0;
					bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
					mu_gvis();
					DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
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
					next_cmpc = EVAL_CMPC(next_rp);
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
				DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
				continue;
			}
			/*
			 * Spanning node-related variables and their usage:
			 *
			 * expected_sn_chunk_number: 	0  - looking for spanning nodes (regular nodes are OK, too)
			 *				!0 - number of the next chunk needed (implies we are building
			 *					a spanning node's value)
			 *
			 * While building a spanning node's value:
			 * numsubs: the number of chunks needed to build the spanning node's value
			 * gblsize: the expected size of the completed value
			 * sn_chunk_number: The chunk number of the chunk from the current record from the extract
			 *
			 * Managing the value
			 * sn_hold_buff: buffer used to accumulate the spanning node's value
			 * sn_hold_buff_size: Allocated size of buffer
			 * sn_hold_buff_pos: amount of the buffer used; where to place the next chunk
			 * sn_hold_buff_temp: used when we have to increase the size of the buffer
			 *
			 * Controlling the placing of the key,value in the database:
			 * ok_to_put: means we are ready to place the key,value in the database, i.e., we have the full value
			 * 		(either of the spanning node or a regular node).
			 * putting_a_sn: we are placing a spanning node in the database, i.e, use the key from sn_gvkey and
			 * 		the value from sn_hold_buff.
			 */
			CHECK_HIDDEN_SUBSCRIPT(gv_currkey,is_hidden_subscript);
			if (!is_hidden_subscript && (max_subsc_len < (gv_currkey->end + 1)))
				max_subsc_len = gv_currkey->end + 1;
			v.str.addr = (char*)cp1;
			v.str.len =INTCAST(rec_len - (cp1 - (unsigned char *)rp));
			if (expected_sn_chunk_number && !is_hidden_subscript)
			{	/* we were expecting a chunk of an spanning node and we did not get one */
				DISPLAY_INCMP_SN_MSG;
				util_out_print("!_!_Expected chunk number : !UL but found a non-spanning node", TRUE,
						expected_sn_chunk_number + 1);
				if (sn_hold_buff_pos)
					DISPLAY_PARTIAL_SN_HOLD_BUFF;
				KILL_INCMP_SN_IF_NEEDED;
				sn_hold_buff_pos = 0;
				expected_sn_chunk_number = 0;
				ok_to_put = TRUE;
				putting_a_sn = FALSE;
				numsubs = 0;
			}
			if (is_hidden_subscript)
			{	/* it's a chunk and we were expecting one */
				sn_chunk_number = SPAN_GVSUBS2INT((span_subs *) &(gv_currkey->base[gv_currkey->end - 4]));
				if (!expected_sn_chunk_number && is_hidden_subscript && sn_chunk_number)
				{ /* we not expecting a payload chunk (as opposed to a control record) but we got one */
					DISPLAY_INCMP_SN_MSG;
					util_out_print("!_!_Not expecting a spanning node chunk but found chunk : !UL", TRUE,
							sn_chunk_number + 1);
					if (v.str.len)
						DISPLAY_VALUE("!_!_Errant Chunk :");
					continue;
				}
				if (0 == sn_chunk_number)
				{ 	/* first spanning node chunk, get ctrl info */
					if (0 != expected_sn_chunk_number)
					{
						DISPLAY_INCMP_SN_MSG;
						util_out_print("!_!_Expected chunk number : !UL but found chunk number : !UL", TRUE,
								expected_sn_chunk_number + 1, sn_chunk_number + 1);
						if (sn_hold_buff_pos)
							DISPLAY_PARTIAL_SN_HOLD_BUFF;
						KILL_INCMP_SN_IF_NEEDED;
					}
					/* start building a new spanning node */
					sn_gvkey->end = gv_currkey->end - (SPAN_SUBS_LEN + 1);
					memcpy(sn_gvkey->base, gv_currkey->base, sn_gvkey->end);
					sn_gvkey->base[sn_gvkey->end] = 0;
					sn_gvkey->prev = gv_currkey->prev;
					sn_gvkey->top = gv_currkey->top;
					GET_NSBCTRL(v.str.addr, numsubs, gblsize);
					/* look for first payload chunk */
					expected_sn_chunk_number = 1;
					sn_hold_buff_pos = 0;
					ok_to_put = FALSE;
					sn_incmp_gbl_already_killed = FALSE;
				} else
				{	/* we only need to compare the key before the hidden subscripts */
					if ((expected_sn_chunk_number == sn_chunk_number)
							&& (sn_gvkey->end == gv_currkey->end - (SPAN_SUBS_LEN + 1))
							&& !memcmp(sn_gvkey->base,gv_currkey->base, sn_gvkey->end)
							&& ((sn_hold_buff_pos + v.str.len) <= gblsize))
					{
						if (NULL == sn_hold_buff)
						{
							sn_hold_buff_size = DEFAULT_SN_HOLD_BUFF_SIZE;
							sn_hold_buff = (char *)malloc(DEFAULT_SN_HOLD_BUFF_SIZE);
						}
						if ((sn_hold_buff_pos + v.str.len) > sn_hold_buff_size)
						{
							sn_hold_buff_size = sn_hold_buff_size * 2;
							sn_hold_buff_temp = (char *)malloc(sn_hold_buff_size);
							memcpy(sn_hold_buff_temp, sn_hold_buff, sn_hold_buff_pos);
							free (sn_hold_buff);
							sn_hold_buff = sn_hold_buff_temp;
						}
						memcpy(sn_hold_buff + sn_hold_buff_pos, v.str.addr, v.str.len);
						sn_hold_buff_pos += v.str.len;
						if (expected_sn_chunk_number == numsubs)
						{
							if (sn_hold_buff_pos != gblsize)
							{	/* we don't have the expected size even though 	*/
								/* we have all the expected chunks.		 		*/
								DISPLAY_INCMP_SN_MSG;
								util_out_print("!_!_Expected size : !UL actual size : !UL", TRUE,
										gblsize, sn_hold_buff_pos);
								if (sn_hold_buff_pos)
									DISPLAY_PARTIAL_SN_HOLD_BUFF;
								KILL_INCMP_SN_IF_NEEDED;
								expected_sn_chunk_number = 0;
								ok_to_put = FALSE;
								sn_hold_buff_pos = 0;
							}
							else
							{
								expected_sn_chunk_number = 0;
								ok_to_put = TRUE;
								putting_a_sn = TRUE;
							}

						}else
							expected_sn_chunk_number++;
					}else
					{
						DISPLAY_INCMP_SN_MSG;
						if ((sn_hold_buff_pos + v.str.len) <= gblsize)
							util_out_print("!_!_Expected chunk number : !UL but found chunk number : !UL", /*BYPASSOK*/
								TRUE, expected_sn_chunk_number + 1, sn_chunk_number + 1);
						else
							util_out_print("!_!_Global value too large:  expected size : !UL actual size : !UL chunk number : !UL", TRUE, /*BYPASSOK*/
								gblsize, sn_hold_buff_pos + v.str.len, sn_chunk_number + 1);
						if (sn_hold_buff_pos)
							DISPLAY_PARTIAL_SN_HOLD_BUFF;
						if (v.str.len)
							DISPLAY_VALUE("!_!_Errant Chunk :");
						KILL_INCMP_SN_IF_NEEDED;
						sn_hold_buff_pos = 0;
						expected_sn_chunk_number = 0;
					}
				}
			} else
				ok_to_put = TRUE;
			if (ok_to_put)
			{
					if (putting_a_sn)
					{
						gv_currkey->base[gv_currkey->end - (SPAN_SUBS_LEN + 1)] = 0;
						gv_currkey->end -= (SPAN_SUBS_LEN + 1);
						v.str.addr = sn_hold_buff;
						v.str.len = sn_hold_buff_pos;
					}
					if (max_data_len < v.str.len)
						max_data_len = v.str.len;
					bin_call_db(BIN_PUT, (INTPTR_T)&v, 0);
					if (mupip_error_occurred)
					{
						if (!mupip_DB_full)
						{
							bin_call_db(ERR_COR, (INTPTR_T)rec_count, (INTPTR_T)global_key_count);
							file_offset = file_offset_base + ((unsigned char *)rp - ptr_base);
							util_out_print("!_!_at File offset : [0x!XL]", TRUE, file_offset);
							DISPLAY_CURRKEY;
							DISPLAY_VALUE("!_!_Value :");
						}
						break;
					}
					if (putting_a_sn)
						putting_a_sn = FALSE;
					else
					{
						key_count++;
						global_key_count++;
					}
			}
		}
	}
	GTMCRYPT_ONLY(
		if (NULL != hash_array)
			free(hash_array);
	)
	free(tmp_gvkey);
	free(sn_gvkey);
	if (NULL != sn_hold_buff)
		free(sn_hold_buff);
	file_input_close();
	util_out_print("LOAD TOTAL!_!_Key Cnt: !UL  Max Subsc Len: !UL  Max Data Len: !UL", TRUE, key_count, max_subsc_len,
			max_data_len);
	util_out_print("Last LOAD record number: !UL\n", TRUE, key_count ? (rec_count - 1) : 0);
	if (mu_ctrly_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LOADCTRLY);
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
			rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_CORRUPT, 2, parm1, parm2);
		case BIN_KILL:
			gvcst_kill(FALSE);
			break;
	}
	REVERT;
}
