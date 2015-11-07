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

GBLREF bool		mupip_error_occurred;
GBLREF bool		mu_ctrly_occurred;
GBLREF bool		mu_ctrlc_occurred;
GBLREF spdesc		stringpool;
GBLREF gv_key		*gv_currkey;
GBLREF sgmnt_addrs	*cs_addrs;

error_def(ERR_LOADCTRLY);
error_def(ERR_LOADEOF);
error_def(ERR_LOADFILERR);
error_def(ERR_LOADINVCHSET);
error_def(ERR_MUNOFINISH);
error_def(ERR_TRIGDATAIGNORE);

#define GO_PUT_SUB		0
#define GO_PUT_DATA		1
#define GO_SET_EXTRACT		2
#define DEFAULT_MAX_REC_SIZE 3096
#define ISSUE_TRIGDATAIGNORE_IF_NEEDED(KEYLENGTH, PTR, HASHT_GBL)								\
{																\
	/* The ordering of the && below is important as the caller uses HASHT_GBL to be set to TRUE if the global pointed to 	\
	 * by PTR is ^#t. 													\
	 */															\
	if ((HASHT_GBL = IS_GVKEY_HASHT_FULL_GBLNAME(KEYLENGTH, PTR)) && !hasht_ignored)					\
	{															\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGDATAIGNORE, 2, KEYLENGTH, PTR);				\
		hasht_ignored = TRUE;												\
	}															\
}

void go_call_db(int routine, char *parm1, int parm2, int val_off1, int val_len1);

void go_load(uint4 begin, uint4 end)
{
	char		*ptr;
	int		len, fmt, keylength, keystate;
	uint4	        iter, max_data_len, max_subsc_len, key_count, max_rec_size;
	mstr            src, des;
	unsigned char   *rec_buff, ch;
	boolean_t	utf8_extract, format_error = FALSE, hasht_ignored = FALSE, hasht_gbl = FALSE;
	char		*val_off;
	int 		val_len, val_off1, val_len1;
	boolean_t	is_setextract;

	gvinit();

	max_rec_size = DEFAULT_MAX_REC_SIZE;
	rec_buff = (unsigned char *)malloc(max_rec_size);

	fmt = MU_FMT_ZWR;	/* by default, the extract format is ZWR (not GO) */
	len = file_input_get(&ptr);
	if (mupip_error_occurred)
	{
		free(rec_buff);
		return;
	}
	if (len >= 0)
	{
		util_out_print("!AD", TRUE, len, ptr);
		utf8_extract = ((len >= STR_LIT_LEN(UTF8_NAME)) &&
				(0 == MEMCMP_LIT(ptr + len - STR_LIT_LEN("UTF-8"), "UTF-8"))) ? TRUE : FALSE;
		if ((utf8_extract && !gtm_utf8_mode) || (!utf8_extract && gtm_utf8_mode))
		{ /* extract CHSET doesn't match $ZCHSET */
			if (utf8_extract)
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LOADINVCHSET, 2, LEN_AND_LIT("UTF-8"));
			else
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_LOADINVCHSET, 2, LEN_AND_LIT("M"));
			mupip_error_occurred = TRUE;
			free(rec_buff);
			return;
		}
	} else
		mupip_exit(ERR_LOADFILERR);
	len = file_input_get(&ptr);
	if (mupip_error_occurred)
	{
		free(rec_buff);
		return;
	}
	if (len >= 0)
	{
		util_out_print("!AD", TRUE, len, ptr);
		fmt = (0 == memcmp(ptr + len - STR_LIT_LEN("ZWR"), "ZWR", STR_LIT_LEN("ZWR"))) ? MU_FMT_ZWR : MU_FMT_GO;
	} else
		mupip_exit(ERR_LOADFILERR);
	if (begin < 3)
		begin = 3;
	for (iter = 3; iter < begin; iter++)
	{
		len = file_input_get(&ptr);
		if (len < 0)	/* The IO device has signalled an end of file */
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_LOADEOF, 1, begin);
			mupip_error_occurred = TRUE;
		}
		if (mupip_error_occurred)
		{
			util_out_print("Error reading record number: !UL\n", TRUE, iter);
			free(rec_buff);
			return;
		}
	}
	assert(iter == begin);
	util_out_print("Beginning LOAD at record number: !UL\n", TRUE, begin);
	max_data_len = 0;
	max_subsc_len = 0;
	key_count = 0;
	for (iter = begin - 1; ; )
	{
		if (++iter > end)
			break;
		if (mu_ctrly_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			util_out_print("!AD:!_  Key cnt: !UL  max subsc len: !UL  max data len: !UL", TRUE,
				       LEN_AND_LIT("LOAD TOTAL"), key_count, max_subsc_len, max_data_len);
			util_out_print("Last LOAD record number: !UL", TRUE, key_count ? iter : 0);
			mu_gvis();
			util_out_print(0, TRUE);
			mu_ctrlc_occurred = FALSE;
		}
		if (0 > (len = file_input_get(&ptr)))
			break;
		if (mupip_error_occurred)
		{
		        mu_gvis();
			break;
		}
		if ('\n' == *ptr)
		{
			if ('\n' == *(ptr+1))
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
			ISSUE_TRIGDATAIGNORE_IF_NEEDED(keylength, ptr, hasht_gbl);
			if (hasht_gbl)
			{
				hasht_gbl = FALSE;
				continue;
			}
			go_call_db(GO_PUT_SUB, ptr, keylength, 0, 0);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				util_out_print("Error loading record number: !UL\n", TRUE, iter);
				mupip_error_occurred = FALSE;
				continue;
			}
			assert(keylength < len - 1);
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			src.len = val_len;
			src.addr = val_off;
			des.len = 0;
			if (src.len > max_rec_size)
			{
			        max_rec_size = src.len;
				free(rec_buff);
				rec_buff = (unsigned char *)malloc(max_rec_size);
			}
			des.addr = (char *)rec_buff;
			if (FALSE == zwr2format(&src, &des))
			{
				util_out_print("Format error in record number !8UL: !/!AD", TRUE, iter, src.len, src.addr);
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
				util_out_print("Error loading record number: !UL\n", TRUE, iter);
				mupip_error_occurred = FALSE;
				continue;
			}
			key_count++;
		} else
		{
			ISSUE_TRIGDATAIGNORE_IF_NEEDED(len, ptr, hasht_gbl);
			if (hasht_gbl)
			{
				if (0 > (len = file_input_get(&ptr)))
					break;
				iter++;
				hasht_gbl = FALSE;
				continue;
			}
		        go_call_db(GO_PUT_SUB, ptr, len, 0, 0);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				util_out_print("Error loading record number: !UL\n", TRUE, iter);
				mupip_error_occurred = FALSE;
				continue;
			}
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			if (++iter > end)
			{
			        iter--;	/* Decrement as didn't load key */
				break;
			}
			if ((len = file_input_get(&ptr)) < 0)
			        break;
			if (mupip_error_occurred)
			{
			        mu_gvis();
				util_out_print("Error loading record number: !UL\n", TRUE, iter);
				break;
			}
			stringpool.free = stringpool.base;
			if (max_data_len < len)
			        max_data_len = len;
			go_call_db(GO_PUT_DATA, ptr, len, 0, 0);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				util_out_print("Error loading record number: !UL\n", TRUE, iter);
				mupip_error_occurred = FALSE;
				continue;
			}
			key_count++;
		}
	}
	free(rec_buff);
	file_input_close();
	if (mu_ctrly_occurred)
	{
		gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_LOADCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
	util_out_print("LOAD TOTAL!_!_Key Cnt: !UL  Max Subsc Len: !UL  Max Data Len: !UL",TRUE,key_count,max_subsc_len,
			max_data_len);
	util_out_print("Last LOAD record number: !UL\n", TRUE, key_count ? (iter - 1) : 0);
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
