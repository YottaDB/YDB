/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
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

#include "stringpool.h"
#include "stp_parms.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "muextr.h"
#include "util.h"
#include "mupip_exit.h"
#include "mlkdef.h"
#include "zshow.h"
#include "file_input.h"
#include "load.h"
#include "mu_gvis.h"
#include "mupip_put_gvdata.h"
#include "mupip_put_gvn_fragment.h"
#include "str2gvkey.h"
#include "gtmmsg.h"
#include "gtm_utf8.h"
#include "gv_trigger.h"
#include "mu_interactive.h"
#include "wbox_test_init.h"
#include "op.h"
#include "io.h"
#include "iormdef.h"
#include "iosp.h"
#include "gtmio.h"
#include "io_params.h"
#include "iotimer.h"
#include "mupip_load_reg_list.h"

GBLREF gd_addr		*gd_header;
GBLREF bool		mupip_error_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mu_ctrlc_occurred;
GBLREF gv_key		*gv_currkey;
GBLREF int		onerror;
GBLREF io_pair		io_curr_device;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF spdesc		stringpool;
GBLREF gd_region	*gv_cur_region;
GBLREF gd_region	*db_init_region;
GBLREF int4             error_condition;

LITREF	mval		literal_notimeout;
LITREF	mval		literal_zero;

error_def(ERR_LOADCTRLY);
error_def(ERR_LOADEOF);
error_def(ERR_LOADFILERR);
error_def(ERR_MUNOFINISH);
error_def(ERR_PREMATEOF);
error_def(ERR_RECLOAD);
error_def(ERR_TRIGDATAIGNORE);
error_def(ERR_FAILEDRECCOUNT);
error_def(ERR_LOADRECCNT);
error_def(ERR_DBFILERR);
error_def(ERR_STATSDBNOTSUPP);
error_def(ERR_JNLFILOPN);
error_def(ERR_DBPRIVERR);
error_def(ERR_GBLOFLOW);

#define GO_PUT_SUB		0
#define GO_PUT_DATA		1
#define GO_SET_EXTRACT		2

STATICFNDEF boolean_t get_mname_from_key(char *ptr, int key_length, char *key, gtm_uint64_t iter,
					gtm_uint64_t first_failed_rec_count, mname_entry *gvname);
#define ISSUE_TRIGDATAIGNORE_IF_NEEDED(KEYLENGTH, PTR, HASHT_GBL, IGNORE)						\
/* The ordering of the && below is important as the caller uses HASHT_GBL to be set to TRUE if the global pointed to 	\
 * by PTR is ^#t. 													\
 */															\
if ((HASHT_GBL = IS_GVKEY_HASHT_FULL_GBLNAME(KEYLENGTH, PTR)) && !IGNORE)						\
{															\
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGDATAIGNORE, 2, KEYLENGTH, PTR);				\
	IGNORE = TRUE;													\
}															\

#define CHECK_MUPIP_ERROR_OCCURRED(MUPIP_ERROR_OCCURRED, ERROR_CONDITION, REG_LIST, NUM_OF_REG, ITER,				\
			FAILED_RECORD_COUNT, FIRST_FAILED_REC_COUNT, MU_LOAD_ERROR, MSG_BUFF)					\
{																\
	if (MUPIP_ERROR_OCCURRED)												\
	{															\
		if (ERR_JNLFILOPN == ERROR_CONDITION || ERR_DBPRIVERR == ERROR_CONDITION ||					\
			ERR_GBLOFLOW == ERROR_CONDITION)									\
		{														\
			insert_reg_to_list(REG_LIST, gv_cur_region, &NUM_OF_REG);						\
			FIRST_FAILED_REC_COUNT = ITER;										\
			MU_LOAD_ERROR = TRUE;											\
		} else														\
		{														\
			FIRST_FAILED_REC_COUNT = 0;										\
			mu_gvis();												\
			SNPRINTF(MSG_BUFF, SIZEOF(MSG_BUFF), "%lld", ITER );							\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(MSG_BUFF));			\
		}														\
		FAILED_RECORD_COUNT++;												\
		ONERROR_PROCESS;												\
	}															\
}																\

STATICFNDEF boolean_t get_mname_from_key(char *ptr, int key_length, char *key, gtm_uint64_t iter,
					gtm_uint64_t first_failed_rec_count, mname_entry *gvname)
{
	int			length, key_name_len;
	char			*ptr1, msg_buff[128];
	gd_region		*reg_ptr = NULL;
	gtm_uint64_t		tmp_rec_count;

	if ('^' == *ptr)
	{
		length = (key_length == 1) ? key_length : key_length - 1;
		for (key_name_len = 0, ptr1 = ptr + 1; ((key_name_len < length) && ('(' != *ptr1)); key_name_len++, ptr1++)
			key[key_name_len] = *ptr1;
		key[key_name_len] = '\0';
	} else
	{
		tmp_rec_count = iter - 1;
		if (tmp_rec_count > first_failed_rec_count)
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld to %lld", first_failed_rec_count, tmp_rec_count );
		else
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld", tmp_rec_count );
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
			return FALSE;
	}
	gvname->var_name.addr = key;
	gvname->var_name.len = MIN(key_name_len , MAX_MIDENT_LEN);
	COMPUTE_HASH_MNAME(gvname);
	return TRUE;
}

void go_load(uint4 begin, uint4 end, unsigned char *rec_buff, char *line3_ptr, int line3_len, uint4 max_rec_size, int fmt,
	int utf8_extract, int dos)
{
	boolean_t	format_error = FALSE, hasht_ignored = FALSE, hasht_gbl = FALSE;
	boolean_t	is_setextract, mu_load_error = FALSE, switch_db, go_format_val_read;
	char		*add_off, *ptr, *val_off;
	gtm_uint64_t	iter, tmp_rec_count, key_count, first_failed_rec_count, failed_record_count, index;
	int		add_len, len, keylength, keystate, val_len, val_len1, val_off1;
	mstr            src, des;
	uint4	        max_data_len, max_subsc_len, num_of_reg;
	char		key[MAX_KEY_SZ], msg_buff[MAX_RECLOAD_ERR_MSG_SIZE];
	gd_region	**reg_list;
	mname_entry	gvname;

	gvinit();
	if ((MU_FMT_GO != fmt) && (MU_FMT_ZWR != fmt))
	{
		assert((MU_FMT_GO == fmt) || (MU_FMT_ZWR == fmt));
		mupip_exit(ERR_LOADFILERR);
	}
	if (begin < 3)
		begin = 3;
	ptr = line3_ptr;
	len = line3_len;
	go_format_val_read = FALSE;
	for (iter = 3; iter < begin; iter++)
	{
		len = go_get(&ptr, 0, max_rec_size);
		if (len < 0)	/* The IO device has signalled an end of file */
		{
			++iter;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_LOADEOF, 1, begin);
			mupip_error_occurred = TRUE;
			util_out_print("Error reading record number: !@UQ\n", TRUE, &iter);
			return;
		}
	}
	assert(iter == begin);
	num_of_reg = 0;
	util_out_print("Beginning LOAD at record number: !UL\n", TRUE, begin);
	des.len = key_count = max_data_len = max_subsc_len = 0;
	des.addr = (char *)rec_buff;
	GTM_WHITE_BOX_TEST(WBTEST_FAKE_BIG_KEY_COUNT, key_count, 4294967196U); /* (2**32)-100=4294967196 */
	reg_list = (gd_region **) malloc(gd_header->n_regions * SIZEOF(gd_region *));
	for (index = 0; index < gd_header->n_regions; index++)
		reg_list[index] = NULL;
	failed_record_count = 0;
	first_failed_rec_count = 0;
	for (iter = begin - 1; ;)
	{
		if (++iter > end)
			break;
		if (mupip_error_occurred && ONERROR_STOP == onerror)
			break;
		if (mu_ctrly_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			util_out_print("!AD:!_  Key cnt: !@UQ  max subsc len: !UL  max data len: !UL", TRUE,
				       LEN_AND_LIT("LOAD TOTAL"), &key_count, max_subsc_len, max_data_len);
			tmp_rec_count = iter - 1;
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_INFO(ERR_LOADRECCNT), 1, &tmp_rec_count);
			mu_gvis();
			util_out_print(0, TRUE);
			mu_ctrlc_occurred = FALSE;
		}
		if ((iter > begin) && (0 > (len = go_get(&ptr, MAX_STRLEN, max_rec_size) - dos)))	/* WARNING assignment */
			break;
		if (mupip_error_occurred)
		{
		        mu_gvis();
			break;
		}
		if ('\n' == *ptr)
		{
			if ('\n' == *(ptr + 1))
				break;
			ptr++;
		}
		if (0 == len)
			continue;
		if (MU_FMT_GO != fmt)
		{
			/* Determine if the ZWR has $extract format */
			if ('$' == *ptr)
			{
				keylength = zwrkeyvallen(ptr, len, &val_off, &val_len, &val_off1, &val_len1);
				ptr = ptr + 4; /* Skip first 4 character '$','z','e','(' */
				is_setextract = TRUE;
			} else
			{
				/* Determine the ZWR key length. -1 (SIZEOF(=)) is needed since ZWR allows '^x(1,2)='*/
				keylength = zwrkeyvallen(ptr, len, &val_off, &val_len, NULL, NULL);
				is_setextract = FALSE;
			}
			if (0 < val_len)
			{
				ISSUE_TRIGDATAIGNORE_IF_NEEDED(keylength, ptr, hasht_gbl, hasht_ignored);
				if (hasht_gbl)
					continue;
			} else
				mupip_error_occurred = TRUE;
			if (mu_load_error)
			{
				if (get_mname_from_key(ptr, keylength, key, iter, first_failed_rec_count, &gvname))
				{
					switch_db = check_db_status_for_global(&gvname, fmt, &failed_record_count, iter,
								 &first_failed_rec_count, reg_list, num_of_reg);
					if (!switch_db)
						continue;
				}
			}
			go_call_db(GO_PUT_SUB, ptr, keylength, 0, 0);
			if (mupip_error_occurred)
			{
				if ((ERR_DBFILERR == error_condition) || (ERR_STATSDBNOTSUPP == error_condition))
				{
					insert_reg_to_list(reg_list, db_init_region, &num_of_reg);
					first_failed_rec_count = iter;
					mu_load_error = TRUE;
				} else
				{
					first_failed_rec_count = 0;
					mu_gvis();
					SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld", iter );
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
					mu_load_error = FALSE;
				}
				failed_record_count++;
				gv_target = NULL;
				gv_currkey->base[0] = '\0';
				ONERROR_PROCESS;
			}
			mu_load_error = FALSE;
			first_failed_rec_count = 0;
			src.len = val_len;
			src.addr = val_off;
			if (src.len > max_rec_size)
			{
				util_out_print("Record too long - record number: !@UQ!/With content:!/!AD",
					TRUE, &iter, src.len, src.addr);
				format_error = TRUE;
				continue;
			}
			des.addr = (char *)rec_buff;
			if (FALSE == zwr2format(&src, &des))
			{
				util_out_print("Format error in record number: !@UQ!/With content:!/!AD",
					TRUE, &iter, src.len, src.addr);
				format_error = TRUE;
				continue;
			}
			(is_setextract) ? go_call_db(GO_SET_EXTRACT, des.addr, des.len, val_off1, val_len1)
					: go_call_db(GO_PUT_DATA, (char *)rec_buff, des.len, 0, 0);
			CHECK_MUPIP_ERROR_OCCURRED(mupip_error_occurred, error_condition, reg_list, num_of_reg, iter,
							failed_record_count, first_failed_rec_count, mu_load_error, msg_buff);
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			if (max_data_len < des.len)
			        max_data_len = des.len;
			des.len = 0;
		} else
		{
			ISSUE_TRIGDATAIGNORE_IF_NEEDED(len, ptr, hasht_gbl, hasht_ignored);
			if (hasht_gbl)
			{
				if (0 > (len = go_get(&ptr, 0, max_rec_size) - dos))	/* WARNING assignment */
					break;
				iter++;
				continue;
			}
			if (mu_load_error)
			{
				if (get_mname_from_key(ptr, len, key, iter, first_failed_rec_count, &gvname))
				{
					switch_db = check_db_status_for_global(&gvname, fmt, &failed_record_count, iter,
								 &first_failed_rec_count, reg_list, num_of_reg);
					if (!switch_db)
					{
						if (++iter > end)
						{
							iter--; /* Decrement as didn't load key */
							break;
						}
						if (0 > (len = go_get(&ptr, 0, max_rec_size) - dos))
							break;
						continue;
					}
				}
			}
			go_call_db(GO_PUT_SUB, ptr, len, 0, 0);
			if (mupip_error_occurred)
			{
				if ((ERR_DBFILERR == error_condition) || (ERR_STATSDBNOTSUPP == error_condition))
				{
					insert_reg_to_list(reg_list, db_init_region, &num_of_reg);
					first_failed_rec_count = iter;
					mu_load_error = TRUE;
					if (++iter > end)
					{
						iter--;	/* Decrement as didn't load key */
						break;
					}
					if (0 > (len = go_get(&ptr, 0, max_rec_size) - dos))		/* WARNING assignment */
						break;
					failed_record_count++;
					go_format_val_read = TRUE;
				} else
				{
					mu_gvis();
					SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld", iter );
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
					mu_load_error = FALSE;
				}
				failed_record_count++;
				gv_target = NULL;
				gv_currkey->base[0] = '\0';
				ONERROR_PROCESS;
			}
			mu_load_error = FALSE;
			first_failed_rec_count = 0;
			if (++iter > end)
			{
				iter--;	/* Decrement as didn't load key */
				break;
			}
			if (0 > (len = go_get(&ptr, 0, max_rec_size) - dos))		/* WARNING assignment */
			        break;
			if (mupip_error_occurred)
			{
				mu_gvis();
				SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld", iter );
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
				break;
			}
			stringpool.free = stringpool.base;
			go_call_db(GO_PUT_DATA, ptr, len, 0, 0);
			CHECK_MUPIP_ERROR_OCCURRED(mupip_error_occurred, error_condition, reg_list, num_of_reg, iter,
						failed_record_count, first_failed_rec_count, mu_load_error, msg_buff);
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			if (max_data_len < len)
			        max_data_len = len;
		}
		key_count++;
	}
	file_input_close();
	free(reg_list);
	if (mu_ctrly_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_LOADCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
	if (mupip_error_occurred && ONERROR_STOP == onerror)
	{
		tmp_rec_count = (go_format_val_read) ? iter - 1 : iter;
		failed_record_count -= (go_format_val_read) ? 1 : 0;
	} else
		tmp_rec_count = iter - 1;
	if (0 != first_failed_rec_count)
	{
		if (tmp_rec_count > first_failed_rec_count)
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld to %lld", first_failed_rec_count , tmp_rec_count);
		else
			SNPRINTF(msg_buff, SIZEOF(msg_buff), "%lld", tmp_rec_count );
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RECLOAD, 2, LEN_AND_STR(msg_buff));
	}
	util_out_print("LOAD TOTAL!_!_Key Cnt: !@UQ  Max Subsc Len: !UL  Max Data Len: !UL",TRUE,&key_count,max_subsc_len,
			max_data_len);
	if (failed_record_count)
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_FAILEDRECCOUNT, 1, &failed_record_count);
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) MAKE_MSG_INFO(ERR_LOADRECCNT), 1, &tmp_rec_count);
	if (failed_record_count)
		mupip_exit(error_condition);
	if (format_error)
		mupip_exit(ERR_LOADFILERR);
}

void go_call_db(int routine, char *parm1, int parm2, int val_off1, int val_len1)
{
	/* In order to duplicate the VMS functionality, which is to trap all errors in mupip_load_ch
	 * and continue in go_load after they occur, it is necessary to call these routines from a
	 * subroutine due to the limitations of condition handlers and unwinding on UNIX.
	 */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH(mupip_load_ch);
	switch(routine)
	{	case GO_PUT_SUB:
			gv_currkey->end = 0;
			str2gvkey_gvfunc(parm1, parm2);
			break;
		case GO_PUT_DATA:
			mupip_put_gvdata(parm1, parm2);
			break;
		case GO_SET_EXTRACT:
			mupip_put_gvn_fragment(parm1, parm2, val_off1, val_len1);
			break;
	}
	REVERT;
}

/* The following is similar to file_get_input in file_input.c but avoids reallocation because the regex memory management issue */
int go_get(char **in_ptr, int max_len, uint4 max_rec_size)
{
	int			rd_len, ret_len;
	mval			val;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH_RET(mupip_load_ch, 0);
	/* one-time only reads if in TP to avoid TPNOTACID, otherwise use untimed reads */
	for (ret_len = 0; ; )
	{
		op_read(&val, (mval *)(dollar_tlevel ? &literal_zero : &literal_notimeout));
		rd_len = val.str.len;
		if ((0 == rd_len) && io_curr_device.in->dollar.zeof)
		{
			REVERT;
			if (io_curr_device.in->dollar.x)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_PREMATEOF);
			return FILE_INPUT_GET_ERROR;
		}
		if ((max_len && (rd_len > max_len)) || ((ret_len + rd_len) > max_rec_size))
		{
			REVERT;
			return FILE_INPUT_GET_LINE2LONG;
		}
		memcpy((unsigned char *)(*in_ptr + ret_len), val.str.addr, rd_len);
		ret_len += rd_len;
		if (!io_curr_device.in->dollar.x)
			break;
	}
	REVERT;
	return ret_len;
}
