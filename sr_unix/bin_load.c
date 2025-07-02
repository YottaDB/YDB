/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "io.h"
#include "gtmcrypt.h"
#include "gv_trigger.h"
#include "gvcst_protos.h"	/* for gvcst_root_search in GV_BIND_NAME_AND_ROOT_SEARCH macro */
#include "format_targ_key.h"
#include "zshow.h"
#include "hashtab_mname.h"
#include "min_max.h"
#include "mu_interactive.h"
#include "mupip_load_reg_list.h"
#include "gvt_inline.h"

#define LAST_NEGATIVE_SUBSCRIPT 127

GBLREF bool		mupip_DB_full;
GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mu_ctrlc_occurred;
GBLREF bool		mupip_error_occurred;
GBLREF spdesc		stringpool;
GBLREF gd_addr		*gd_header;
GBLREF gv_key		*gv_altkey;
GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF gd_region	*gv_cur_region;
GBLREF int4		gv_keysize;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF int		onerror;
GBLREF io_pair		io_curr_device;
GBLREF gd_region	*db_init_region;
GBLREF int4		error_condition;
GBLREF boolean_t	tref_transform;

LITREF boolean_t mu_int_possub[16][16];
LITREF boolean_t mu_int_negsub[16][16];
LITREF boolean_t mu_int_exponent[256];

enum err_code
{
	CORRUPTNODE,
	DBBADNSUB,
	DBCMPNZRO,
	DBKEYMX,
	DBRSIZMN,
	DBRSIZMX,
	GVINVALID,
	KEY2BIG,
	MAXNRSUBSCRIPTS,
	REC2BIG
};

#define	BIN_PUT		0
#define	BIN_BIND	1
#define	ERR_COR		2
#define	BIN_KILL	3
#define	BIN_PUT_GVSPAN	4
#define	TEXT1		"Record discarded because"

# define FREE_MALLOCS						\
{								\
	int i;							\
	for (i = 0; i < ARRAYSIZE(malloc_fields); i++)		\
	{							\
		if (NULL != *(malloc_fields[i]))		\
			free(*(malloc_fields[i]));		\
	}							\
}

# define GC_BIN_LOAD_ERR(GTMCRYPT_ERRNO)											\
{																\
	io_log_name		*io_log;											\
																\
	if (0 != GTMCRYPT_ERRNO)												\
	{															\
		io_log = io_curr_device.in->name;										\
		GTMCRYPT_REPORT_ERROR(GTMCRYPT_ERRNO, gtm_putmsg, io_log->len, io_log->dollar_io);				\
		mupip_error_occurred = TRUE;											\
		FREE_MALLOCS;													\
		return;														\
	}															\
}

#define	DEFAULT_SN_HOLD_BUFF_SIZE MAX_IO_BLOCK_SIZE

#define KILL_INCMP_SN_IF_NEEDED(GVNH_REG)											\
{																\
	gd_region	*dummy_reg;												\
																\
	if (!sn_incmp_gbl_already_killed)											\
	{															\
		COPY_KEY(sn_savekey, gv_currkey);										\
		COPY_KEY(gv_currkey, sn_gvkey);											\
		/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH						\
		 * (e.g. setting gv_cur_region for spanning globals).								\
		 */														\
		GV_BIND_SUBSNAME_IF_GVSPAN(GVNH_REG, gd_header, gv_currkey, dummy_reg);						\
		bin_call_db(BIN_KILL, 0, 0, 0, 0, NULL);									\
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
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_LDSPANGLOINCMP, 1, &file_offset);					\
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
	file_offset = file_offset_base + ((unsigned char *)rp - ptr_base);							\
	util_out_print("!_!_File offset : [0x!16@XQ]", TRUE, &file_offset);							\
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

gvnh_reg_t	*bin_call_db(int, int, INTPTR_T, INTPTR_T, int, unsigned char*);
void		zwr_out_print(char * buff, int len);

#define ZWR_BASE_STRIDE 1024
#define UOP_BASE_STRIDE 1024

void zwr_out_print(char * buff, int bufflen)
{
	char zwrbuff[ZWR_EXP_RATIO(ZWR_BASE_STRIDE)], savechar;
	int zwrlen;
	int buffpos, left, stride;
	int uopbuffpos, uopleft, uopstride;

	buffpos = 0;
	while ((left = bufflen - buffpos))
	{
		if (buffpos)
			FPRINTF(stderr,"_");
		stride = (left > ZWR_BASE_STRIDE) ? ZWR_BASE_STRIDE : left;
		zwrlen = SIZEOF(zwrbuff);
		format2zwr((sm_uc_ptr_t)(buff + buffpos), stride, (unsigned char *)zwrbuff, &zwrlen);
		uopbuffpos = 0;
		while ((uopleft = zwrlen - uopbuffpos))
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

void bin_load(gtm_uint64_t begin, gtm_uint64_t end, char *line1_ptr, int line1_len)
{
	unsigned char		*ptr, *cp1, *cp2, *btop, *gvkey_char_ptr, *tmp_ptr, *tmp_key_ptr, *c, *ctop, *ptr_base;
	unsigned char		hdr_lvl, src_buff[MAX_KEY_SZ + 1], dest_buff[MAX_ZWR_KEY_SZ],
				cmpc_str[MAX_KEY_SZ + 1], dup_key_str[MAX_KEY_SZ + 1], sn_key_str[MAX_KEY_SZ + 1], *sn_key_str_end;
	unsigned char		*end_buff, *gvn_char, *subs, mych;
	unsigned short		rec_len, next_cmpc, numsubs = 0, num_subscripts;
	int			len, current, last, max_key = 0, max_rec = 0, fmtd_key_len;
	int			tmp_cmpc, sn_chunk_number, expected_sn_chunk_number = 0;
	int			sn_hold_buff_pos = 0, sn_hold_buff_size = 0;
	int			i;
	uint4			max_data_len, max_subsc_len, gblsize = 0, data_len, num_of_reg = 0;
	ssize_t			subsc_len, extr_std_null_coll;
	gtm_uint64_t		iter, key_count, tmp_rec_count, global_key_count;
	DEBUG_ONLY(gtm_uint64_t		saved_begin = 0);
	gtm_uint64_t		first_failed_rec_count, failed_record_count;
	off_t			last_sn_error_offset = 0, file_offset_base = 0, file_offset = 0;
	boolean_t		need_xlation, new_gvn;
	boolean_t		is_hidden_subscript, ok_to_put = TRUE, putting_a_sn = FALSE, sn_incmp_gbl_already_killed = FALSE;
	rec_hdr			*rp, *next_rp;
	mval			v, tmp_mval, *val = NULL;
	mname_entry		gvname;
	mstr			mstr_src, mstr_dest, opstr;
	collseq			*extr_collseq, *db_collseq, *save_gv_target_collseq;
	coll_hdr		extr_collhdr = { 0, 0, 0, 0,}, db_collhdr = { 0, 0, 0, 0 };
	bool			extr_collhdr_set = FALSE, db_collhdr_set = FALSE;
	gv_key			*tmp_gvkey, *sn_gvkey, *sn_savekey, *save_orig_key;
	gv_key			*orig_gv_currkey_ptr = NULL;
	char			std_null_coll[BIN_HEADER_NUMSZ + 1], *sn_hold_buff = NULL, *sn_hold_buff_temp;
	int			in_len, gtmcrypt_errno, n_index, encrypted_hash_array_len, null_iv_array_len;
	char			*inbuf, *encrypted_hash_array_ptr, *curr_hash_ptr, *null_iv_array_ptr = NULL, null_iv_char;
	int4			index;
	gtmcrypt_key_t		*encr_key_handles;
	boolean_t		encrypted_version, mixed_encryption = FALSE, valid_gblname;
	char			index_err_buf[1024];
	gvnh_reg_t		*gvnh_reg;
	gd_region		*dummy_reg, *reg_ptr = NULL;
	sub_num			subtocheck;
	sgmnt_data_ptr_t	csd;
	boolean_t		discard_nullcoll_mismatch_record;
	unsigned char		subscript, *r_ptr;
	unsigned int		null_subscript_cnt, k, sub_index[MAX_GVSUBSCRIPTS];
	static unsigned char	key_buffer[MAX_ZWR_KEY_SZ];
	unsigned char		*temp, coll_typr_char;
	boolean_t		switch_db, mu_load_error = FALSE;
	gd_binding		*map;
	ht_ent_mname		*tabent;
	hash_table_mname	*tab_ptr;
	char			msg_buff[MAX_RECLOAD_ERR_MSG_SIZE];
	gd_region		**reg_list;
	/* Array to help track malloc'd storage during this routine and release prior to error out or return */
	char			**malloc_fields[] = {
		(char **)&reg_list,
		(char **)&tmp_gvkey,
		(char **)&sn_gvkey,
		(char **)&sn_savekey,
		(char **)&save_orig_key,
		(char **)&encrypted_hash_array_ptr,
		(char **)&encr_key_handles,
		(char **)&null_iv_array_ptr,
		&sn_hold_buff
	};
#	ifdef DEBUG
	unsigned char		*save_msp;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (i = 0; i < ARRAYSIZE(malloc_fields); i++)
		*(malloc_fields[i]) = NULL;		/* Make sure all pointers are cleared to start */
	assert(4 == SIZEOF(coll_hdr));
	gvinit();
	reg_list = (gd_region **)malloc(gd_header->n_regions * SIZEOF(gd_region *));
	for (index = 0; index < gd_header->n_regions; index++)
		reg_list[index] = NULL;
	v.mvtype = MV_STR;
	file_offset_base = 0;
	/* line1_ptr & line1_len are initialized as part of get_load_format using a read of the binary extract file which
	 * did not go through file_input_bin_get. So initialize the internal static structures that file_input_bin_get
	 * maintains as if that read happened through it. This will let us finish reading the binary extract header line.
	 */
	file_input_bin_init(line1_ptr, line1_len);
	len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base, DO_RTS_ERROR_FALSE);
	if (0 >= len)
	{
		FREE_MALLOCS;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_LDBINFMT);
		mupip_exit(ERR_LDBINFMT);
	}
	hdr_lvl = EXTR_HEADER_LEVEL(ptr);
	if (!(	((('4' == hdr_lvl) || ('5' == hdr_lvl)) && (V5_BIN_HEADER_SZ == len)) ||
		((('6' <= hdr_lvl) && ('9' >= hdr_lvl)) && (BIN_HEADER_SZ == len)) ||
		(('4' > hdr_lvl) && (V3_BIN_HEADER_SZ == len))))
	{
		FREE_MALLOCS;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_LDBINFMT);
		mupip_exit(ERR_LDBINFMT);
	}
	/* expecting the level in a single character */
	assert(' ' == *(ptr + SIZEOF(BIN_HEADER_LABEL) - 3));
	if (0 != memcmp(ptr, BIN_HEADER_LABEL, SIZEOF(BIN_HEADER_LABEL) - 2) || ('2' > hdr_lvl) ||
			*(BIN_HEADER_VERSION_ENCR_IV) < hdr_lvl)
	{	/* ignore the level check */
		FREE_MALLOCS;
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_LDBINFMT);
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
		} else
		{
			memcpy(std_null_coll, ptr + V5_BIN_HEADER_NULLCOLLOFFSET, V5_BIN_HEADER_NUMSZ);
			std_null_coll[V5_BIN_HEADER_NUMSZ] = '\0';
		}
		extr_std_null_coll = STRTOUL(std_null_coll, NULL, 10);
		if (0 != extr_std_null_coll && 1!= extr_std_null_coll)
		{
			FREE_MALLOCS;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2,
				      RTS_ERROR_TEXT("Corrupted null collation field in header"), ERR_LDBINFMT);
			mupip_exit(ERR_LDBINFMT);
		}
	} else
		extr_std_null_coll = 0;
	/* Encrypted versions to date. */
	encrypted_version = ('5' <= hdr_lvl) && ('6' != hdr_lvl); /* Includes 5, 7, 8, and 9. */
	if (encrypted_version)
	{
		encrypted_hash_array_len = file_input_bin_get((char **)&ptr, &file_offset_base,
								(char **)&ptr_base, DO_RTS_ERROR_TRUE);
		encrypted_hash_array_ptr = malloc(encrypted_hash_array_len);
		memcpy(encrypted_hash_array_ptr, ptr, encrypted_hash_array_len);
		n_index = encrypted_hash_array_len / GTMCRYPT_HASH_LEN;
		encr_key_handles = (gtmcrypt_key_t *)malloc(SIZEOF(gtmcrypt_key_t) * n_index);
		memset(encr_key_handles, 0, SIZEOF(gtmcrypt_key_t) * n_index);
		INIT_PROC_ENCRYPTION(gtmcrypt_errno);
		GC_BIN_LOAD_ERR(gtmcrypt_errno);
		mixed_encryption = FALSE;
		for (index = 0; index < n_index; index++)
		{
			curr_hash_ptr = encrypted_hash_array_ptr + index * GTMCRYPT_HASH_LEN;
			if (0 == memcmp(curr_hash_ptr, EMPTY_GTMCRYPT_HASH, GTMCRYPT_HASH_LEN))
			{
				mixed_encryption = TRUE;
				continue;
			}
			GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(NULL, curr_hash_ptr, 0, NULL, encr_key_handles[index], gtmcrypt_errno);
			GC_BIN_LOAD_ERR(gtmcrypt_errno);
		}
		if ('9' == hdr_lvl)
		{
			null_iv_array_len = file_input_bin_get((char **)&ptr, &file_offset_base,
									(char **)&ptr_base, DO_RTS_ERROR_TRUE);
			assert(n_index == null_iv_array_len);
			null_iv_array_ptr = malloc(null_iv_array_len);
			memcpy(null_iv_array_ptr, ptr, null_iv_array_len);
		}
	} else
	{
		encrypted_hash_array_ptr = NULL;
		n_index = 0;
	}
	if ('2' < hdr_lvl)
	{
		len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base, DO_RTS_ERROR_TRUE);
		if (SIZEOF(coll_hdr) != len)
		{
			FREE_MALLOCS;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Corrupt collation header"),
				      ERR_LDBINFMT);
			mupip_exit(ERR_LDBINFMT);
		}
		extr_collhdr = *((coll_hdr *)(ptr));
		extr_collhdr_set = TRUE;
		new_gvn = TRUE;
	} else
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_OLDBINEXTRACT, 1, hdr_lvl - '0');
	if (begin < 2) /* WARNING: begin can never be less than 2 */
		begin = 2;
#ifdef DEBUG
	if ((WBTEST_ENABLED(WBTEST_FAKE_BIG_KEY_COUNT)) && (2UL < begin))
		saved_begin = begin, begin = 2;
	else if (WBTEST_ENABLED(WBTEST_FAKE_BIG_KEY_COUNT))
		saved_begin = FAKE_BIG_KEY_COUNT;
#endif
	for (iter = 2; iter < begin; iter++)
	{
		if (!(len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base, DO_RTS_ERROR_TRUE)))
		{
			FREE_MALLOCS;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_LOADEOF, 1, &begin);
			util_out_print("Error reading record number: !@UQ\n", TRUE, &iter);
			mupip_error_occurred = TRUE;
			return;
		} else if (len == SIZEOF(coll_hdr))
		{
			extr_collhdr = *((coll_hdr *)(ptr));
			extr_collhdr_set = TRUE;
			assert(hdr_lvl > '2');
			iter--;
		}
	}
	assert(NULL == tmp_gvkey);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(tmp_gvkey, DBKEYSIZE(MAX_KEY_SZ));	/* tmp_gvkey will point to malloced memory after this */
	assert(NULL == sn_gvkey);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(sn_gvkey, DBKEYSIZE(MAX_KEY_SZ));	/* sn_gvkey will point to malloced memory after this */
	assert(NULL == sn_savekey);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(sn_savekey, DBKEYSIZE(MAX_KEY_SZ));	/* sn_gvkey will point to malloced memory after this */
	assert(NULL == save_orig_key);	/* GVKEY_INIT macro relies on this */
	GVKEY_INIT(save_orig_key, DBKEYSIZE(MAX_KEY_SZ));	/* sn_gvkey will point to malloced memory after this */
	assert(iter == begin);
	global_key_count = key_count = 0;
	max_data_len = max_subsc_len = 0;
	extr_collseq = db_collseq = NULL;
	gvnh_reg = NULL;
	failed_record_count = 0;
	first_failed_rec_count = 0;
	val = NULL;
	need_xlation = FALSE;
	GTM_WHITE_BOX_TEST(WBTEST_FAKE_BIG_KEY_COUNT, key_count, saved_begin);
	GTM_WHITE_BOX_TEST(WBTEST_FAKE_BIG_KEY_COUNT, begin, saved_begin);
	iter = begin - 1; /* WARNING: iter can never be zero because begin can never be less than 2 */
	util_out_print("Beginning LOAD at record number: !@UQ\n", TRUE, &begin);
	while (!mupip_DB_full)
	{
		if ((++iter > end) || (0 == iter))
			break;
		next_cmpc = 0;
		if (mupip_error_occurred && ONERROR_STOP == onerror)
			break;
		mupip_error_occurred = FALSE;
		if (mu_ctrly_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			util_out_print("!AD:!_  Key cnt: !@UQ  max subsc len: !UL  max data len: !UL", TRUE,
				LEN_AND_LIT("LOAD TOTAL"), &key_count, max_subsc_len, max_data_len);
			tmp_rec_count = (iter == begin) ? iter : iter - 1;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_INFO(ERR_LOADRECCNT), 1, &tmp_rec_count);
			mu_gvis();
			util_out_print(0, TRUE);
			mu_ctrlc_occurred = FALSE;
		}
		if (!(len = file_input_bin_get((char **)&ptr, &file_offset_base, (char **)&ptr_base, DO_RTS_ERROR_TRUE))
				|| mupip_error_occurred)
			break;
		else if (len == SIZEOF(coll_hdr))
		{
			extr_collhdr = *((coll_hdr *)(ptr));
			extr_collhdr_set = TRUE;
			assert(hdr_lvl > '2');
			new_gvn = TRUE;			/* next record will contain a new gvn */
			iter--;	/* Decrement as this record does not count as a record for loading purposes */
			continue;
		}
		if (encrypted_version)
		{	/* Getting index value from the extracted file. It indicates which database file this record belongs to */
			GET_LONG(index, ptr);
			tmp_ptr = ptr + SIZEOF(int4);
			if (-1 != index)
			{
				switch (hdr_lvl)
				{
					case '9':
						/* Record is encrypted; ensure legitimate encryption handle index. */
						assert(n_index);
						if ((n_index <= index) || (0 > index))
						{
							SNPRINTF(index_err_buf, SIZEOF(index_err_buf),
								"Encryption handle expected in the range [0; %d) but found %d",
								n_index, index);
							SNPRINTF(msg_buff, SIZEOF(msg_buff), "%" PRIu64, iter );
							send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) MAKE_MSG_SEVERE(ERR_RECLOAD),
								2, LEN_AND_STR(msg_buff), ERR_TEXT, 2,
								RTS_ERROR_TEXT(index_err_buf), ERR_GVFAILCORE);
							gtm_fork_n_core();
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) MAKE_MSG_SEVERE(ERR_RECLOAD),
								2, LEN_AND_STR(msg_buff), msg_buff, ERR_TEXT, 2,
								RTS_ERROR_TEXT(index_err_buf));
							FREE_MALLOCS;
							return;
						}
#						ifdef DEBUG
						/* Ensure that len is the length of one int, block header, and all records. */
						GET_ULONG(data_len, &((blk_hdr *)tmp_ptr)->bsiz);
						assert(data_len + SIZEOF(int4) == len);
#						endif
						in_len = len - SIZEOF(int4) - SIZEOF(blk_hdr);
						inbuf = (char *)(tmp_ptr + SIZEOF(blk_hdr));
						assert(null_iv_array_ptr);
						null_iv_char = null_iv_array_ptr[index];
						assert(('1' == null_iv_char) || ('0' == null_iv_char));
						GTMCRYPT_DECRYPT(NULL, ('0' == null_iv_char), encr_key_handles[index],
								inbuf, in_len, NULL, tmp_ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
						GC_BIN_LOAD_ERR(gtmcrypt_errno);
						rp = (rec_hdr *)(tmp_ptr + SIZEOF(blk_hdr));
						break;
					case '7':
						/* In version 7 the extract logic did not properly distinguish non-encrypted records
						 * from encrypted ones in case both kinds were present. Specifically, extracts did
						 * not precede unencrypted records with a '-1' to signify that no decryption is
						 * required. Here we make the best attempt to recognize that situation and process
						 * the record so long as the index appears legitimate.
						 *
						 * By now we know that the record is probably encrypted; however, there is a slight
						 * chance that the encryption handle index is incorrect, although in a legitimate
						 * range. In that case we will additionally check if the corresponding hash is
						 * non-zero---which would only be the case if the region was encrypted---but only
						 * when dealing with a mix of encrypted and unencrypted data.
						 */
						assert(n_index);
						assert(encrypted_version);
						assert(encrypted_hash_array_ptr);
						if ((n_index <= index) || (0 > index)
						    || (mixed_encryption
							&& (0 == memcmp(encrypted_hash_array_ptr + index * GTMCRYPT_HASH_LEN,
									EMPTY_GTMCRYPT_HASH, GTMCRYPT_HASH_LEN))))
						{
							rp = (rec_hdr *)ptr;
							break;
						}
							/* CAUTION: Fall-through. */
					case '5':	/* CAUTION: Fall-through. */
					case '8':
						/* Record is encrypted; ensure legitimate encryption handle index. */
						assert(n_index);
						assertpro((n_index > index) && (0 <= index));
						in_len = len - SIZEOF(int4);
						inbuf = (char *)tmp_ptr;
						GTMCRYPT_DECRYPT_NO_IV(NULL, encr_key_handles[index],
								inbuf, in_len, NULL, gtmcrypt_errno);
						GC_BIN_LOAD_ERR(gtmcrypt_errno);
						rp = (rec_hdr *)tmp_ptr;
						break;
					default:
						assertpro(hdr_lvl != hdr_lvl);
				}
			} else
				rp = (rec_hdr *)tmp_ptr;
		} else
			rp = (rec_hdr *)(ptr);
		btop = ptr + len;
		cp1 = (unsigned char *)(rp + 1);
		gvname.var_name.addr = (char*)cp1;
		while (*cp1++)
			;
		gvname.var_name.len = INTCAST((char *)cp1 - gvname.var_name.addr - 1);
		if (('2' >= hdr_lvl) || new_gvn)
		{
			if ((HASHT_GBLNAME_LEN == gvname.var_name.len)
					&& (0 == memcmp(gvname.var_name.addr, HASHT_GBLNAME, HASHT_GBLNAME_LEN)))
				continue;
			gvname.var_name.len = MIN(gvname.var_name.len, MAX_MIDENT_LEN);
			COMPUTE_HASH_MNAME(&gvname);
			if (mu_load_error)
			{
				switch_db = check_db_status_for_global(&gvname, MU_FMT_BINARY, &failed_record_count, iter,
								&first_failed_rec_count, reg_list, num_of_reg);
				if (!switch_db)
					continue;
			}
			gvnh_reg = bin_call_db(BIN_BIND, 0, (INTPTR_T)gd_header, (INTPTR_T)&gvname, 0, NULL);
			/* "gv_cur_region" will be set at this point in case the global does NOT span regions.
			 * For globals that do span regions, "gv_cur_region" will be set just before the call to op_gvput.
			 * This value of "gvnh_reg" will be in effect until all records of this global are processed.
			 */
			if (mupip_error_occurred)
			{
				if ((ERR_DBFILERR == error_condition) || (ERR_STATSDBNOTSUPP == error_condition))
				{
					insert_reg_to_list(reg_list, db_init_region, &num_of_reg);
					first_failed_rec_count = iter;
					mu_load_error = TRUE;
				} else
				{
					failed_record_count = 0;
					SNPRINTF(msg_buff, SIZEOF(msg_buff), "%" PRIu64, iter);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
				}
				failed_record_count++;
				ONERROR_PROCESS;
			}
			mu_load_error= FALSE;
			first_failed_rec_count = 0;
			max_key = gvnh_reg->gd_reg->max_key_size;
			max_rec = gvnh_reg->gd_reg->max_rec_size;
			db_collhdr.act = gv_target->act;
			db_collhdr.ver = gv_target->ver;
			db_collhdr.nct = gv_target->nct;
			db_collhdr_set = TRUE;
		}
		GET_USHORT(rec_len, &rp->rsiz);
		if (0 != EVAL_CMPC(rp))
		{
			bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
			bin_call_db(ERR_COR, DBCMPNZRO, 0, 0, gvname.var_name.len, (unsigned char*)(gvname.var_name.addr));
		} else if (gvname.var_name.len > rec_len)
		{
			bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
			bin_call_db(ERR_COR, DBKEYMX, 0, 0, gvname.var_name.len, (unsigned char*)(gvname.var_name.addr));
		} else if (mupip_error_occurred)
		{
			bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
		}
		if (mupip_error_occurred)
		{
			mu_gvis();
			DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
			continue;
		}
		assert(!new_gvn || (extr_collhdr_set && db_collhdr_set));
		if (new_gvn && extr_collhdr_set && db_collhdr_set)
		{
			global_key_count = 1;
			if ((db_collhdr.act != extr_collhdr.act) || (db_collhdr.ver != extr_collhdr.ver)
				|| (db_collhdr.nct != extr_collhdr.nct) || (gvnh_reg->gd_reg->std_null_coll != extr_std_null_coll))
			{
				if (extr_collhdr.act)
				{
					if ((extr_collseq = ready_collseq((int)extr_collhdr.act)))
					{
						if (!do_verify(extr_collseq, extr_collhdr.act, extr_collhdr.ver))
						{
							FREE_MALLOCS;
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_COLLTYPVERSION, 2,
								       extr_collhdr.act, extr_collhdr.ver, ERR_GVIS, 2,
								       gv_altkey->end - 1, gv_altkey->base);
							mupip_exit(ERR_COLLTYPVERSION);
						}
					} else
					{
						FREE_MALLOCS;
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_COLLATIONUNDEF, 1,
							       extr_collhdr.act, ERR_GVIS, 2, gv_altkey->end - 1, gv_altkey->base);
						mupip_exit(ERR_COLLATIONUNDEF);
					}
				}
				if (db_collhdr.act)
				{
					if ((db_collseq = ready_collseq((int)db_collhdr.act)))
					{
						if (!do_verify(db_collseq, db_collhdr.act, db_collhdr.ver))
						{
							FREE_MALLOCS;
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_COLLTYPVERSION, 2,
								       db_collhdr.act, db_collhdr.ver, ERR_GVIS, 2,
								       gv_altkey->end - 1, gv_altkey->base);
							mupip_exit(ERR_COLLTYPVERSION);
						}
					} else
					{
						FREE_MALLOCS;
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
		for ( ; rp < (rec_hdr*)btop; rp = (rec_hdr*)((unsigned char *)rp + rec_len))
		{
			csd = cs_data;
			GET_USHORT(rec_len, &rp->rsiz);
			if (rec_len + (unsigned char *)rp > btop)
			{
				bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
				bin_call_db(ERR_COR, DBRSIZMX, 0, 0, gvname.var_name.len, (unsigned char*)(gvname.var_name.addr));
				mu_gvis();
				DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
				break;
			}
			cp1 = (unsigned char*)(rp + 1);
			cp2 = gv_currkey->base + EVAL_CMPC(rp);
			current = 1;
			for ( ; ; )
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
					bin_call_db(ERR_COR, CORRUPTNODE, (INTPTR_T)iter, (INTPTR_T)global_key_count,
							0, NULL);
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
				assert(extr_collhdr.act || db_collhdr.act || extr_collhdr.nct || db_collhdr.nct
						|| (extr_std_null_coll != gvnh_reg->gd_reg->std_null_coll));
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
				COPY_KEY(save_orig_key, gv_currkey);
				gvkey_char_ptr = dup_key_str;
				while (*gvkey_char_ptr++)
					;
				gv_currkey->prev = 0;
				gv_currkey->end = gvkey_char_ptr - dup_key_str;
				assert(gv_keysize <= tmp_gvkey->top);
				while (*gvkey_char_ptr)
				{
						/* get next subscript (in GT.M internal subsc format) */
					tmp_ptr = src_buff;
					while (*gvkey_char_ptr)
						*tmp_ptr++ = *gvkey_char_ptr++;
					subsc_len = tmp_ptr - src_buff;
					src_buff[subsc_len] = '\0';
					if (extr_collseq)
					{
						/* undo the extract time collation */
						tref_transform = TRUE;
						save_gv_target_collseq = gv_target->collseq;
						gv_target->collseq = extr_collseq;
					} else
						tref_transform = FALSE;
						/* convert the subscript to string format */
					opstr.addr = (char *)dest_buff;
					opstr.len = MAX_ZWR_KEY_SZ;
					end_buff = gvsub2str(src_buff, &opstr, FALSE);
						/* transform the string to the current subsc format */
					tref_transform = TRUE;
					tmp_mval.mvtype = MV_STR;
					tmp_mval.str.addr = (char *)dest_buff;
					tmp_mval.str.len = INTCAST(end_buff - dest_buff);
					tmp_gvkey->prev = 0;
					tmp_gvkey->end = 0;
					if (extr_collseq)
						gv_target->collseq = save_gv_target_collseq;
					mval2subsc(&tmp_mval, tmp_gvkey, gvnh_reg->gd_reg->std_null_coll);
						/* we now have the correctly transformed subscript */
					tmp_key_ptr = gv_currkey->base + gv_currkey->end;
					memcpy(tmp_key_ptr, tmp_gvkey->base, tmp_gvkey->end + 1);
					gv_currkey->prev = gv_currkey->end;
					gv_currkey->end += tmp_gvkey->end;
					gvkey_char_ptr++;
				}
			}
			assert(max_key);
			if (gv_currkey->end >= max_key)
			{
				bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
				bin_call_db(ERR_COR, KEY2BIG, gv_currkey->end, max_key, REG_LEN_STR(gvnh_reg->gd_reg));
				mu_gvis();
				DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
				continue;
			}
			/* Validate the global variable name */
			gvn_char = gv_currkey->base;
			valid_gblname = VALFIRSTCHAR(*gvn_char);
			gvn_char++; /* need to post-increment here since above macro uses it multiple times */
			for (; (*gvn_char && valid_gblname) ; gvn_char++)
				valid_gblname = valid_gblname && VALKEY(*gvn_char);
			valid_gblname = (valid_gblname && (KEY_DELIMITER == *gvn_char));
			if (!valid_gblname)
			{
				mupip_error_occurred = TRUE;
				bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
				bin_call_db(ERR_COR, GVINVALID, 0, 0, (gvn_char - gv_currkey->base), gv_currkey->base);
				mu_gvis();
				DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
				break;
			}
			/* Validate the subscripts */
			subs = gvn_char + 1;
			num_subscripts = 0;
			while ((mych = *subs++)) /* WARNING: assignment */
			{
				num_subscripts++;
				if (MAX_GVSUBSCRIPTS < num_subscripts)
				{
					mupip_error_occurred = TRUE;
					bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
					bin_call_db(ERR_COR, MAXNRSUBSCRIPTS, 0, 0, 0, NULL);
					mu_gvis();
					DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
					break;
				}
				if (mu_int_exponent[mych]) /* Is it a numeric subscript? */
				{
					if (mych > LAST_NEGATIVE_SUBSCRIPT) /* Is it a positive numeric subscript */
					{
						while ((mych = *subs++)) /* WARNING: assignment */
						{
							memcpy(&subtocheck, &mych, 1);
							if (!mu_int_possub[subtocheck.one][subtocheck.two])
							{
								mupip_error_occurred = TRUE;
								bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
								bin_call_db(ERR_COR, DBBADNSUB, 0, 0,
										(gvn_char - gv_currkey->base), gv_currkey->base);
								mu_gvis();
								DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
								break;
							}
						}
					} else /* It is a negative subscript */
					{
						while ((mych = *subs++) && (STR_SUB_PREFIX != mych)) /* WARNING: assignment */
						{
							memcpy(&subtocheck, &mych, 1);
							if (!mu_int_negsub[subtocheck.one][subtocheck.two])
							{
								mupip_error_occurred = TRUE;
								bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
								bin_call_db(ERR_COR, DBBADNSUB, 0, 0,
										(gvn_char - gv_currkey->base), gv_currkey->base);
								mu_gvis();
								DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
								break;
							}
						}
						if (!mupip_error_occurred && ((STR_SUB_PREFIX != mych) || (*subs)))
						{
							mupip_error_occurred = TRUE;
							bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
							mu_gvis();
							DISPLAY_FILE_OFFSET_OF_RECORD_AND_REST_OF_BLOCK;
							break;
						}
					}
				}
				else /* It is a string subscript */
				{
					/* string can have arbitrary content so move to the next subscript */
					while ((mych = *subs++)) /* WARNING: assignment */
						;
				}
				if (mupip_error_occurred)
					break;
			}
			if (mupip_error_occurred)
				break;
			/*
			 * Spanning node-related variables and their usage:
			 *
			 * expected_sn_chunk_number:	0  - looking for spanning nodes (regular nodes are OK, too)
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
			 *		(either of the spanning node or a regular node).
			 * putting_a_sn: we are placing a spanning node in the database, i.e, use the key from sn_gvkey and
			 *		the value from sn_hold_buff.
			 */
			CHECK_HIDDEN_SUBSCRIPT(gv_currkey,is_hidden_subscript);
			v.str.addr = (char*)cp1;
			v.str.len =INTCAST(rec_len - (cp1 - (unsigned char *)rp));
			if (expected_sn_chunk_number && !is_hidden_subscript)
			{	/* we were expecting a chunk of an spanning node and we did not get one */
				DISPLAY_INCMP_SN_MSG;
				util_out_print("!_!_Expected chunk number : !UL but found a non-spanning node", TRUE,
						expected_sn_chunk_number + 1);
				if (sn_hold_buff_pos)
					DISPLAY_PARTIAL_SN_HOLD_BUFF;
				KILL_INCMP_SN_IF_NEEDED(gvnh_reg);
				sn_hold_buff_pos = 0;
				expected_sn_chunk_number = 0;
				ok_to_put = TRUE;
				putting_a_sn = FALSE;
				numsubs = 0;
				gblsize = 0;
			}
			if (is_hidden_subscript)
			{	/* it's a chunk and we were expecting one */
				sn_chunk_number = SPAN_GVSUBS2INT((span_subs *) &(gv_currkey->base[gv_currkey->end - 4]));
				if (!expected_sn_chunk_number && sn_chunk_number)
				{ /* we not expecting a payload chunk (as opposed to a control record) but we got one */
					DISPLAY_INCMP_SN_MSG;
					util_out_print("!_!_Not expecting a spanning node chunk but found chunk : !UL", TRUE,
							sn_chunk_number + 1);
					if (v.str.len)
						DISPLAY_VALUE("!_!_Errant Chunk :");
					continue;
				}
				if (0 == sn_chunk_number)
				{	/* first spanning node chunk, get ctrl info */
					if (0 != expected_sn_chunk_number)
					{
						DISPLAY_INCMP_SN_MSG;
						util_out_print("!_!_Expected chunk number : !UL but found chunk number : !UL", TRUE,
								expected_sn_chunk_number + 1, sn_chunk_number + 1);
						if (sn_hold_buff_pos)
							DISPLAY_PARTIAL_SN_HOLD_BUFF;
						KILL_INCMP_SN_IF_NEEDED(gvnh_reg);
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
					assert(gblsize);
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
							free(sn_hold_buff);
							sn_hold_buff = sn_hold_buff_temp;
						}
						memcpy(sn_hold_buff + sn_hold_buff_pos, v.str.addr, v.str.len);
						sn_hold_buff_pos += v.str.len;
						if (expected_sn_chunk_number == numsubs)
						{
							if (sn_hold_buff_pos != gblsize)
							{	/* we don't have the expected size even though	*/
								/* we have all the expected chunks.		*/
								DISPLAY_INCMP_SN_MSG;
								util_out_print("!_!_Expected size : !UL actual size : !UL", TRUE,
										gblsize, sn_hold_buff_pos);
								if (sn_hold_buff_pos)
									DISPLAY_PARTIAL_SN_HOLD_BUFF;
								KILL_INCMP_SN_IF_NEEDED(gvnh_reg);
								expected_sn_chunk_number = 0;
								ok_to_put = FALSE;
								sn_hold_buff_pos = 0;
							} else
							{
								expected_sn_chunk_number = 0;
								ok_to_put = TRUE;
								putting_a_sn = TRUE;
							}

						} else
							expected_sn_chunk_number++;
					} else
					{
						DISPLAY_INCMP_SN_MSG;
						if ((sn_hold_buff_pos + v.str.len) <= gblsize)
							util_out_print("!_!_Expected chunk number : "
								"!UL but found chunk number : !UL",
								TRUE, expected_sn_chunk_number + 1, sn_chunk_number + 1);
						else
							util_out_print("!_!_Global value too large:  expected size : "
								"!UL actual size : !UL chunk number : !UL", TRUE,
								gblsize, sn_hold_buff_pos + v.str.len, sn_chunk_number + 1);
						if (sn_hold_buff_pos)
							DISPLAY_PARTIAL_SN_HOLD_BUFF;
						if (v.str.len)
							DISPLAY_VALUE("!_!_Errant Chunk :");
						KILL_INCMP_SN_IF_NEEDED(gvnh_reg);
						sn_hold_buff_pos = 0;
						expected_sn_chunk_number = 0;
					}
				}
			} else
				ok_to_put = TRUE;
			if (ok_to_put)
			{
				if (!need_xlation)
					COPY_KEY(save_orig_key, gv_currkey);
				orig_gv_currkey_ptr = gv_currkey;
				gv_currkey = save_orig_key;
				for (r_ptr = gv_currkey->base; *r_ptr != KEY_DELIMITER; r_ptr++)
					;
				null_subscript_cnt = 0;
				coll_typr_char = (gvnh_reg->gd_reg->std_null_coll) ? SUBSCRIPT_STDCOL_NULL : STR_SUB_PREFIX;
				for (;;)
				{
					if (r_ptr >= (gv_currkey->base + gv_currkey->end))
						break;
					if (KEY_DELIMITER == *r_ptr++)
					{
						subscript = *r_ptr;
						if (KEY_DELIMITER == subscript)
							break;
						if (SUBSCRIPT_ZERO == subscript || KEY_DELIMITER != *(r_ptr + 1))
						{
							r_ptr++;
							continue;
						}
						sub_index[null_subscript_cnt++] = (r_ptr - gv_currkey->base);
					}
				}
				if (0 < null_subscript_cnt && !csd->null_subs)
				{
					temp = (unsigned char *)format_targ_key(&key_buffer[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
					fmtd_key_len = (int)(temp - key_buffer);
					key_buffer[fmtd_key_len] = '\0';
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_NULSUBSC, 4, LEN_AND_LIT(TEXT1),
						DB_LEN_STR(gv_cur_region), ERR_GVIS, 2, fmtd_key_len, &key_buffer[0]);
					ok_to_put = FALSE;
				}
				discard_nullcoll_mismatch_record = FALSE;
				if (0 < null_subscript_cnt && gv_target->root)
				{
					if (NULL == val)
					{	/* Push an mv_stent in the M-stack so we are safe from "stp_gcol" */
						PUSH_MV_STENT(MVST_MVAL);
						DEBUG_ONLY(save_msp = msp);
						val = &mv_chain->mv_st_cont.mvs_mval;
					}
					if ((csd->std_null_coll ? SUBSCRIPT_STDCOL_NULL
								: STR_SUB_PREFIX) != gv_currkey->base[sub_index[0]])
					{
						for (k = 0; k < null_subscript_cnt; k++)
							gv_currkey->base[sub_index[k]] = coll_typr_char;
						if (gvcst_get(val))
							discard_nullcoll_mismatch_record = TRUE;
						for (k = 0; k < null_subscript_cnt; k++)
							gv_currkey->base[sub_index[k]] = (csd->std_null_coll) ? STR_SUB_PREFIX
										: SUBSCRIPT_STDCOL_NULL;
					}
				}
				if (discard_nullcoll_mismatch_record)
				{
					temp = (unsigned char *)format_targ_key(key_buffer, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
					fmtd_key_len = (int)(temp - key_buffer);
					key_buffer[fmtd_key_len] = '\0';
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBDUPNULCOL, 4,
						LEN_AND_STR(key_buffer), v.str.len, v.str.addr);
					ok_to_put = FALSE;
				}
				gv_currkey = orig_gv_currkey_ptr;
			}
			if (ok_to_put)
			{
				if (gvnh_reg->gd_reg->null_subs)
				{
					for (k = 0; k < null_subscript_cnt; k++)
						gv_currkey->base[sub_index[k]] = csd->std_null_coll
							? SUBSCRIPT_STDCOL_NULL : STR_SUB_PREFIX;
				}
				if (putting_a_sn)
				{
					gv_currkey->base[gv_currkey->end - (SPAN_SUBS_LEN + 1)] = 0;
					gv_currkey->end -= (SPAN_SUBS_LEN + 1);
					v.str.addr = sn_hold_buff;
					v.str.len = sn_hold_buff_pos;
				}
				assert(max_rec);
				if (v.str.len > max_rec)
				{
					bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
					bin_call_db(ERR_COR, REC2BIG, v.str.len, max_rec, REG_LEN_STR(gvnh_reg->gd_reg));
					assert(mupip_error_occurred);
					break;
				}
				if (gvnh_reg->gvspan)
					bin_call_db(BIN_PUT_GVSPAN, 0, (INTPTR_T)&v, (INTPTR_T)gvnh_reg, 0, NULL);
				else
					bin_call_db(BIN_PUT, 0, (INTPTR_T)&v, 0, 0, NULL);
				if (mupip_error_occurred)
				{
					if ((ERR_JNLFILOPN == error_condition) || (ERR_DBPRIVERR == error_condition) ||
						(ERR_GBLOFLOW == error_condition))
					{
						first_failed_rec_count = iter;
						insert_reg_to_list(reg_list, gv_cur_region, &num_of_reg);
						mu_load_error = TRUE;
						failed_record_count++;
						break;
					} else
					{
						if (ERR_DBFILERR == error_condition)
							failed_record_count++;
						bin_call_db(ERR_COR, CORRUPTNODE, iter, global_key_count, 0, NULL);
						file_offset = file_offset_base + ((unsigned char *)rp - ptr_base);
						util_out_print("!_!_at File offset : [0x!16@XQ]", TRUE, &file_offset);
						DISPLAY_CURRKEY;
						DISPLAY_VALUE("!_!_Value :");
					}
					ONERROR_PROCESS;
				}
				mu_load_error = FALSE;
				if (!is_hidden_subscript && (max_subsc_len < (gv_currkey->end + 1)))
					max_subsc_len = gv_currkey->end + 1;
				if (max_data_len < v.str.len)
					max_data_len = v.str.len;
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
	if (NULL != val)
	{
		assert(msp == save_msp);
		POP_MV_STENT();
	}
	if (encrypted_version)
	{
		assert(NULL != encrypted_hash_array_ptr);
		if ('9' == hdr_lvl)
		{
			assert(NULL != null_iv_array_ptr);
		}
		assert(NULL != encr_key_handles);
	}
	FREE_MALLOCS;
	file_input_close();
	tmp_rec_count = (iter == begin) ? iter : iter - 1;
	if (0 != first_failed_rec_count)
	{
		if (tmp_rec_count > first_failed_rec_count)
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%" PRIu64 " to %" PRIu64,
					first_failed_rec_count, tmp_rec_count);
		else
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%" PRIu64, tmp_rec_count);
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
	}
	util_out_print("LOAD TOTAL!_!_Key Cnt: !@UQ  Max Subsc Len: !UL  Max Data Len: !UL", TRUE,
			&key_count, max_subsc_len, max_data_len);
	if (failed_record_count)
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FAILEDRECCOUNT, 1, &failed_record_count);
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_INFO(ERR_LOADRECCNT), 1, &tmp_rec_count);
	if (failed_record_count)
		mupip_exit(error_condition);
	if (mu_ctrly_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LOADCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
}

gvnh_reg_t *bin_call_db(int routine, int err_code, INTPTR_T parm1, INTPTR_T parm2, int strlen, unsigned char* str)
{	/* In order to duplicate the VMS functionality, which is to trap all errors in mupip_load_ch and
	 * continue in bin_load after they occur, it is necessary to call these routines from a
	 * subroutine due to the limitations of condition handlers and unwinding on UNIX
	 */
	gvnh_reg_t	*gvnh_reg = NULL;
	gd_region	*dummy_reg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_RET(mupip_load_ch, gvnh_reg);
	switch (routine)
	{
		case BIN_PUT_GVSPAN:
			/* The below macro finishes the task of GV_BIND_NAME_AND_ROOT_SEARCH
			 * (e.g. setting gv_cur_region for spanning globals).
			 */
			gvnh_reg = (gvnh_reg_t *)parm2;
			GV_BIND_SUBSNAME_IF_GVSPAN(gvnh_reg, gd_header, gv_currkey, dummy_reg);
			/* WARNING: fall-through */
		case BIN_PUT:
			op_gvput((mval *)parm1);
			break;
		case BIN_BIND:
			GV_BIND_NAME_AND_ROOT_SEARCH((gd_addr *)parm1, (mname_entry *)parm2, gvnh_reg);
			break;
		case ERR_COR:
			/* We use rts_error_csa instead of gtm_putmsg_csa here even though we are eating the error with
			 * mupip_load_ch.  This takes advantage of logic mupip_load_ch for cleaning up gv_target.
			 * mupip_load_ch also sets the global mupip_error_occurred to TRUE, which is used in the main loop
			 * of bin_load to indicate there was a problem with the current global node and to move onto the next
			 * global node.
			 */
			switch(err_code)
			{
				case DBBADNSUB:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBBADNSUB, 2, strlen, str);
					break;
				case DBCMPNZRO:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBCMPNZRO, 2, strlen, str);
					break;
				case DBKEYMX:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBKEYMX, 2, strlen, str);
					break;
				case DBRSIZMN:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBRSIZMN, 2, strlen, str);
					break;
				case DBRSIZMX:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_DBRSIZMX, 2, strlen, str);
					break;
				case GVINVALID:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_GVINVALID, 2, strlen, str);
					break;
				case KEY2BIG:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(6) ERR_KEY2BIG, 4, parm1, parm2, strlen, str);
					break;
				case MAXNRSUBSCRIPTS:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(1) ERR_MAXNRSUBSCRIPTS);
					break;
				case REC2BIG:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(6) ERR_REC2BIG, 4, parm1, parm2, strlen, str);
					break;
				default:
					RTS_ERROR_CSA_ABT(cs_addrs, VARLSTCNT(4) ERR_CORRUPTNODE, 2, parm1, parm2);
			}
		case BIN_KILL:
			gvcst_kill(FALSE);
			break;
	}
	REVERT;
	return gvnh_reg;
}
