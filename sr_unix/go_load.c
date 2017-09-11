/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include <rtnhdr.h>
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

GBLREF bool		mupip_error_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mu_ctrlc_occurred;
GBLREF gv_key		*gv_currkey;
GBLREF int		onerror;
GBLREF io_pair		io_curr_device;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF spdesc		stringpool;

LITREF	mval		literal_notimeout;
LITREF	mval		literal_zero;

error_def(ERR_LOADCTRLY);
error_def(ERR_LOADEOF);
error_def(ERR_LOADFILERR);
error_def(ERR_MUNOFINISH);
error_def(ERR_PREMATEOF);
error_def(ERR_RECLOAD);
error_def(ERR_TRIGDATAIGNORE);

#define GO_PUT_SUB		0
#define GO_PUT_DATA		1
#define GO_SET_EXTRACT		2

#define ISSUE_TRIGDATAIGNORE_IF_NEEDED(KEYLENGTH, PTR, HASHT_GBL, IGNORE)						\
/* The ordering of the && below is important as the caller uses HASHT_GBL to be set to TRUE if the global pointed to 	\
 * by PTR is ^#t. 													\
 */															\
if ((HASHT_GBL = IS_GVKEY_HASHT_FULL_GBLNAME(KEYLENGTH, PTR)) && !IGNORE)						\
{															\
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGDATAIGNORE, 2, KEYLENGTH, PTR);				\
	IGNORE = TRUE;													\
}															\

void go_load(uint4 begin, uint4 end, unsigned char *rec_buff, char *line3_ptr, int line3_len, uint4 max_rec_size, int fmt,
	int utf8_extract, int dos)
{
	boolean_t	format_error = FALSE, hasht_ignored = FALSE, hasht_gbl = FALSE;
	boolean_t	is_setextract;
	char		*add_off, *ptr, *val_off;
	gtm_uint64_t	iter, tmp_rec_count, key_count;
	int		add_len, len, keylength, keystate, val_len, val_len1, val_off1;
	mstr            src, des;
	uint4	        max_data_len, max_subsc_len;

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
	util_out_print("Beginning LOAD at record number: !UL\n", TRUE, begin);
	des.len = key_count = max_data_len = max_subsc_len = 0;
	des.addr = (char *)rec_buff;
	GTM_WHITE_BOX_TEST(WBTEST_FAKE_BIG_KEY_COUNT, key_count, 4294967196U); /* (2**32)-100=4294967196 */
	for (iter = begin - 1; ; )
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
			tmp_rec_count = key_count ? iter : 0;
			util_out_print("Last LOAD record number: !@UQ", TRUE, &tmp_rec_count);
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
			go_call_db(GO_PUT_SUB, ptr, keylength, 0, 0);
			if (mupip_error_occurred)
			{
				mu_gvis();
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RECLOAD, 1, &iter);
				gv_target = NULL;
				gv_currkey->base[0] = '\0';
				ONERROR_PROCESS;
			}
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
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
			if (max_data_len < des.len)
			        max_data_len = des.len;
			(is_setextract) ? go_call_db(GO_SET_EXTRACT, des.addr, des.len, val_off1, val_len1)
					: go_call_db(GO_PUT_DATA, (char *)rec_buff, des.len, 0, 0);
			if (mupip_error_occurred)
			{
				mu_gvis();
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RECLOAD, 1, &iter);
				ONERROR_PROCESS;
			}
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
			go_call_db(GO_PUT_SUB, ptr, len, 0, 0);
			if (mupip_error_occurred)
			{
				mu_gvis();
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RECLOAD, 1, &iter);
				gv_target = NULL;
				gv_currkey->base[0] = '\0';
				ONERROR_PROCESS;
			}
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
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
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RECLOAD, 1, &iter);
				break;
			}
			stringpool.free = stringpool.base;
			if (max_data_len < len)
			        max_data_len = len;
			go_call_db(GO_PUT_DATA, ptr, len, 0, 0);
			if (mupip_error_occurred)
			{
				mu_gvis();
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RECLOAD, 1, &iter);
				ONERROR_PROCESS;
			}
		}
		key_count++;
	}
	file_input_close();
	if (mu_ctrly_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_LOADCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
	util_out_print("LOAD TOTAL!_!_Key Cnt: !@UQ  Max Subsc Len: !UL  Max Data Len: !UL",TRUE,&key_count,max_subsc_len,
			max_data_len);
	tmp_rec_count = key_count ? (iter - 1) : 0;
	util_out_print("Last LOAD record number: !@UQ\n", TRUE, &tmp_rec_count);
	if (format_error)
		mupip_exit(ERR_LOADFILERR);
}

void go_call_db(int routine, char *parm1, int parm2, int val_off1, int val_len1)
{
	/* In order to duplicate the VMS functionality, which is to trap all errors in mupip_load_ch
	 * and continue in go_load after they occur, it is necessary to call these routines from a
	 * subroutine due to the limitations of condition handlers and unwinding on UNIX.
	 */
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
