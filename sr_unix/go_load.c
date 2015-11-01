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

#include <string.h>

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
#include "mu_load_input.h"
#include "load.h"
#include "mu_gvis.h"
#include "mupip_put_gvdata.h"
#include "mupip_put_gvsubsc.h"
#include "gtmmsg.h"

GBLREF bool	mupip_error_occurred;
GBLREF bool	mu_ctrly_occurred;
GBLREF bool	mu_ctrlc_occurred;
GBLREF spdesc	stringpool;
GBLREF gv_key	*gv_currkey;

#define GO_PUT_SUB	     0
#define GO_PUT_DATA	     1
#define DEFAULT_MAX_REC_SIZE 3096

static readonly unsigned char gt_lit[] = "LOAD TOTAL";
void go_call_db(int routine, char *parm1, int parm2);

void go_load(int begin, int end)
{
	char		*ptr;
	int		i, len, fmt, keylength, keystate;
	uint4	        max_data_len, max_subsc_len, key_count, rec_count, max_rec_size;
	mstr            src, des;
	unsigned char   *rec_buff, ch;
	boolean_t	format_error = FALSE, keepgoing;

	error_def(ERR_LOADFILERR);
	error_def(ERR_MUNOFINISH);
	error_def(ERR_LOADCTRLY);

	gvinit();

	max_rec_size = DEFAULT_MAX_REC_SIZE;
	rec_buff = (unsigned char *)malloc(max_rec_size);

	if (!begin)
	{
		len = mu_load_get(&ptr);
		if (mupip_error_occurred)
		{
		        free(rec_buff);
		        return;
		}
		if (len >= 0)
		        util_out_print("!AD", TRUE, len, ptr);
		else
			mupip_exit(ERR_LOADFILERR);
		len = mu_load_get(&ptr);
		if (mupip_error_occurred)
		{
		        free(rec_buff);
			return;
		}
		if (len >= 0)
		{
		        util_out_print("!AD", TRUE, len, ptr);
			if (0 == memcmp(ptr + len - sizeof("ZWR") + 1, "ZWR", sizeof("ZWR") - 1))
			        fmt = MU_FMT_ZWR;
			else
			        fmt = MU_FMT_GO;
		} else
			mupip_exit(ERR_LOADFILERR);
		begin = 3;
	} else
	{
	        for (i = 1; i < begin; i++)
		{
		        len = mu_load_get(&ptr);
			if (mupip_error_occurred)
			{
			        free(rec_buff);
				return;
			}
			if (len < 0)
				break;
			if (2 == i) /* the format flag appears only in the second record. */
			{
			        if (0 == memcmp(ptr + len - sizeof("ZWR") + 1, "ZWR", sizeof("ZWR") - 1))
				        fmt = MU_FMT_ZWR;
				else
				        fmt = MU_FMT_GO;
			}
		}
		util_out_print("Beginning LOAD at #!UL\n", TRUE, begin);
	}
	max_data_len = 0;
	max_subsc_len = 0;
	key_count = 0;
	rec_count = 1;

	for (i = begin - 1 ; ;)
	{
		if (++i > end)
			break;
		if (mu_ctrly_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			util_out_print("!AD:!_  Key cnt: !UL  max subsc len: !UL  max data len: !UL", TRUE,
				LEN_AND_LIT(gt_lit), key_count, max_subsc_len, max_data_len);
			util_out_print("Last LOAD record number: !UL", TRUE, rec_count - 1);
			mu_gvis();
			util_out_print(0, TRUE);
			mu_ctrlc_occurred = FALSE;
		}
		if ((len = mu_load_get(&ptr)) < 0)
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
		stringpool.free = stringpool.base;
		rec_count++;
		if (0 == len)
			continue;
		if (MU_FMT_GO != fmt)
		{
		        keylength = 0;					/* determine length of key */
			keystate  = 0;
			keepgoing = TRUE;
			while((keylength < len - 1) && keepgoing)	/* 1 == sizeof(=), since ZWR allows '^x(1,2)='*/
			{
			        ch = *(ptr + keylength);
				keylength++;
				switch (keystate)
				{
				case 0:						/* in global name */
				        if ('=' == ch)				/* end of key */
					{
					        keylength--;
						keepgoing = FALSE;
					} else if ('(' == ch)			/* start of subscripts */
					        keystate = 1;
					break;
				case 1:						/* in subscript area, but out of "..." or $C(...) */
					switch (ch)
					{
					case ')':					/* end of subscripts ==> end of key */
						assert('=' == *(ptr + keylength));
						keepgoing = FALSE;
						break;
					case '"':					/* step into "..." */
						keystate = 2;
						break;
					case '$':					/* step into $C(...) */
						assert(('C' == *(ptr + keylength)) || ('c' == *(ptr + keylength)));
						assert('(' == *(ptr + keylength + 1));
						keylength += 2;
						keystate = 3;
						break;
					}
					break;
				case 2:						/* in "..." */
					if ('"' == ch)
					{
						switch (*(ptr + keylength))
						{
						case '"':				/* "" */
							keylength++;
							break;
						case '_':				/* _$C(...) */
							assert('$' == *(ptr + keylength + 1));
							assert(('c' == *(ptr + keylength + 2)) || ('C' == *(ptr + keylength + 2)));
							assert('(' == *(ptr + keylength + 3));
							keylength += 4;
							keystate = 3;
							break;
						default:				/* step out of "..." */
							keystate = 1;
						}
					}
					break;
				case 3:						/* in $C(...) */
					if (')' == ch)
					{
						if ('_' == *(ptr + keylength))		/* step into "..." */
						{
							assert('"' == *(ptr + keylength + 1));
							keylength += 2;
							keystate = 2;
							break;
						} else
							keystate = 1;			/* step out of $C(...) */
					}
					break;
				default:
					assert(FALSE);
					break;
				}
			}
			assert(keylength < len - 1);
			go_call_db(GO_PUT_SUB, ptr, keylength);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				mupip_error_occurred = FALSE;
				continue;
			}
			if (max_subsc_len < gv_currkey->end)
			        max_subsc_len = gv_currkey->end;
			src.len = len - keylength - 1;
			src.addr = (char *)(ptr + keylength + 1);
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
				util_out_print("Format error in record !8UL: !/!AD", TRUE, rec_count + 1,
					src.len, src.addr);
				format_error = TRUE;
				continue;
			}
			if (max_data_len < des.len)
			        max_data_len = des.len;
			stringpool.free = stringpool.base;
			go_call_db(GO_PUT_DATA, (char *)rec_buff, des.len);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				mupip_error_occurred = FALSE;
				continue;
			}
			key_count++;
		} else
		{
		        go_call_db(GO_PUT_SUB, ptr, len);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				mupip_error_occurred = FALSE;
				continue;
			}
			if (max_subsc_len < gv_currkey->end)
			        max_subsc_len = gv_currkey->end;
			if (++i > end)
			{
			        i--;	/* Decrement as didn't load key */
				break;
			}
			if ((len = mu_load_get(&ptr)) < 0)
			        break;
			if (mupip_error_occurred)
			{
			        mu_gvis();
				break;
			}
			rec_count++;
			stringpool.free = stringpool.base;
			if (max_data_len < len)
			        max_data_len = len;
			go_call_db(GO_PUT_DATA, ptr, len);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				mupip_error_occurred = FALSE;
				continue;
			}
			key_count++;
		}
	}
	free(rec_buff);
	mu_load_close();
	if(mu_ctrly_occurred)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_LOADCTRLY);
		mupip_exit(ERR_MUNOFINISH);
	}
	util_out_print("LOAD TOTAL!_!_Key Cnt: !UL  Max Subsc Len: !UL  Max Data Len: !UL",TRUE,key_count,max_subsc_len,
			max_data_len);
	util_out_print("Last LOAD record number: !UL\n",TRUE,i - 1);
	if (format_error)
		mupip_exit(ERR_LOADFILERR);
}

void go_call_db(int routine, char *parm1, int parm2)
{
	/* In order to duplicate the VMS functionality, which is to trap all errors in mupip_load_ch
	 * and continue in go_load after they occur, it is necessary to call these routines from a
	 * subroutine due to the limitations of condition handlers and unwinding on UNIX.
	 */
	ESTABLISH(mupip_load_ch);
	switch(routine)
	{	case GO_PUT_SUB:
			mupip_put_gvsubsc(parm1, parm2, TRUE);
			break;
		case GO_PUT_DATA:
			mupip_put_gvdata(parm1 , parm2);
			break;
	}
	REVERT;
}
