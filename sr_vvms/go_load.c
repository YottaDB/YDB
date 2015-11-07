/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rms.h>

#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "error.h"
#include "msg.h"
#include "muextr.h"
#include "util.h"
#include "mupip_exit.h"
#include "mlkdef.h"
#include "zshow.h"
#include "mu_load_stat.h"
#include "load.h"
#include "mu_gvis.h"
#include "mupip_put_gvdata.h"
#include "str2gvkey.h"
#include "gtmmsg.h"
#include "iottdefsp.h"

#define DEFAULT_MAX_REC_SIZE  3072

GBLREF bool	mupip_error_occurred;
GBLREF bool	mu_ctrly_occurred;
GBLREF bool	mu_ctrlc_occurred;
GBLREF gd_addr	*gd_header;
GBLREF spdesc   stringpool;
GBLREF gv_key	*gv_currkey;
GBLREF gd_region *gv_cur_region;

error_def(ERR_RECCNT);
error_def(ERR_MUPIPINFO);
error_def(ERR_PREMATEOF);
error_def(ERR_LOADABORT);
error_def(ERR_LOADCTRLY);
error_def(ERR_LOADEOF);
error_def(ERR_BEGINST);
error_def(ERR_LOADFILERR);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);
error_def(ERR_SYSCALL);

/***********************************************************************************************/
/*				GT.M Go Format or ZWR Format                                   */
/***********************************************************************************************/
void go_load(uint4 begin, uint4 end, struct RAB *inrab, struct FAB *infab)
{
	uint4	max_data_len, max_subsc_len, max_record_size, status;
	uint4	key_count, rec_count;
	msgtype	msg;
	char	*ptr;
	int	len, keylength, keystate, fmt = MU_FMT_ZWR;
	mstr	src, des;
	boolean_t	keepgoing, format_error = FALSE;
	unsigned char	*rec_buff, ch;

	msg.new_opts = msg.def_opts = 1;
	inrab->rab$w_usz = ZWR_EXP_RATIO(gd_header->max_rec_size);
	inrab->rab$l_ubf = malloc(inrab->rab$w_usz + 8);
	inrab->rab$l_ubf = (((int4)(inrab->rab$l_ubf) + 7) & -8);
	max_data_len = 0;
	max_subsc_len = 0;
	key_count = 0;
	rec_count = 1;
	max_record_size = DEFAULT_MAX_REC_SIZE;
	rec_buff = (unsigned char *)malloc(max_record_size);
	for (; 3 > rec_count; rec_count++)
	{
		status = sys$get(inrab);		/* scan off header */
		if (RMS$_EOF == status)
		{
			sys$close(infab);
			mupip_exit(ERR_PREMATEOF);
		}
		if (RMS$_NORMAL != status)
		{
			gtm_putmsg(VARLSTCNT(14) ERR_LOADFILERR, 2, infab->fab$b_fns, infab->fab$l_fna,
					ERR_SYSCALL, 5, LEN_AND_LIT("SYS$GET"), CALLFROM, status, 0, inrab->rab$l_stv);
			sys$close(infab);
			mupip_exit(ERR_MUNOACTION);
		}
		len = inrab->rab$w_rsz;
		ptr = inrab->rab$l_rbf;
		while (0 < len && ((ASCII_LF == *(ptr + len - 1)) || (ASCII_CR == *(ptr + len - 1))))
			len--;
		if (2 == rec_count)	/* the format flag appears only in the second record. */
			fmt = (0 == memcmp(ptr + len - STR_LIT_LEN("ZWR"), "ZWR", STR_LIT_LEN("ZWR"))) ?
				MU_FMT_ZWR : MU_FMT_GO;
		msg.msg_number = ERR_MUPIPINFO;
		msg.arg_cnt = 6;
		msg.fp_cnt = 4;
		msg.fp[0].n = len;
		msg.fp[1].cp = ptr;
		sys$putmsg(&msg, 0, 0, 0);
	}
	if (begin < 3)
		begin = 3;
	assert(3 == rec_count);
	for (; rec_count < begin; rec_count++)			/* scan to begin */
	{
		status = sys$get(inrab);
		if (RMS$_EOF == status)
		{
			sys$close(infab);
			gtm_putmsg(VARLSTCNT(3) ERR_LOADEOF, 1, begin);
			mupip_exit(ERR_MUNOACTION);
		}
		if (RMS$_NORMAL != status)
		{
			gtm_putmsg(VARLSTCNT(14) ERR_LOADFILERR, 2, infab->fab$b_fns, infab->fab$l_fna,
					ERR_SYSCALL, 5, LEN_AND_LIT("SYS$GET"), CALLFROM, status, 0, inrab->rab$l_stv);
			sys$close(infab);
			mupip_exit(ERR_MUNOACTION);
		}
	}
	msg.msg_number = ERR_BEGINST;
	msg.arg_cnt = 4;
	msg.fp_cnt = 2;
	msg.fp[0].n = rec_count;
	sys$putmsg(&msg, 0, 0, 0);

	ESTABLISH(mupip_load_ch);
	for (; ; rec_count++)
	{
		if (mu_ctrly_occurred || mupip_error_occurred)
			break;
		if (mu_ctrlc_occurred)
		{
			mu_load_stat(max_data_len, max_subsc_len, key_count, key_count ? (rec_count - 1) : 0, ERR_RECCNT);
			mu_gvis();
			util_out_print("", TRUE);
		}
		/* reset the stringpool for every record in order to avoid garbage collection */
		stringpool.free = stringpool.base;
		if (rec_count > end)
			break;
		if (RMS$_EOF == (status = sys$get(inrab)))
			break;
		if (RMS$_NORMAL != status)
		{
			rts_error(VARLSTCNT(14) ERR_LOADFILERR, 2, infab->fab$b_fns, infab->fab$l_fna,
					ERR_SYSCALL, 5, LEN_AND_LIT("SYS$GET"), CALLFROM, status, 0, inrab->rab$l_stv);
			mupip_error_occurred = TRUE;
			break;
		}
		len = inrab->rab$w_rsz;
		ptr = inrab->rab$l_rbf;
		while (0 < len && ((ASCII_LF == *(ptr + len - 1)) || (ASCII_CR == *(ptr + len - 1))))
			len--;
		if (0 == len)
			continue;
		if (MU_FMT_ZWR == fmt)
		{
		        keylength = 0;
			keystate  = 0;
			keepgoing = TRUE;
			while((keylength < len - 1) && keepgoing) 	/* 1 == SIZEOF(=), since ZWR allows '^x(1,2)='*/
			{
			        ch = *(ptr + keylength);
				keylength++;
				switch (keystate)
				{
				case 0:					/* in global name */
				        if ('=' == ch)			/* end of key */
					{
					        keylength--;
						keepgoing = FALSE;
					}
					else if ('(' == ch)		/* start of subscripts */
					        keystate = 1;
					break;
				case 1:					/* in subscripts area, but out of quotes or $C() */
                                        switch (ch)
                                        {
                                        case ')':                                       /* end of subscripts ==> end of key */
                                                assert('=' == *(ptr + keylength));
                                                keepgoing = FALSE;
                                                break;
                                        case '"':                                       /* step into "..." */
                                                keystate = 2;
                                                break;
                                        case '$':                                       /* step into $C(...) */
                                                assert(('C' == *(ptr + keylength)) || ('c' == *(ptr + keylength)));
                                                assert('(' == *(ptr + keylength + 1));
                                                keylength += 2;
                                                keystate = 3;
                                                break;
                                        }
                                        break;
                                case 2:                                         /* in "..." */
                                        if ('"' == ch)
                                        {
                                                switch (*(ptr + keylength))
                                                {
                                                case '"':                               /* "" */
                                                        keylength++;
                                                        break;
                                                case '_':                               /* _$C(...) */
                                                        assert('$' == *(ptr + keylength + 1));
                                                        assert(('c' == *(ptr + keylength + 2)) || ('C' == *(ptr + keylength + 2)));
                                                        assert('(' == *(ptr + keylength + 3));
                                                        keylength += 4;
                                                        keystate = 3;
                                                        break;
                                                default:                                /* step out of "..." */
                                                        keystate = 1;
                                                }
                                        }
                                        break;
                                case 3:                                         /* in $C(...) */
                                        if (')' == ch)
                                        {
                                                if ('_' == *(ptr + keylength))          /* step into "..." */
                                                {
                                                        assert('"' == *(ptr + keylength + 1));
                                                        keylength += 2;
                                                        keystate = 2;
                                                        break;
                                                }
                                                else
                                                        keystate = 1;                   /* step out of $C(...) */
                                        }
                                        break;
                                default:
                                        assert(FALSE);
                                        break;
                                }
			}
			gv_currkey->end = 0;
			str2gvkey_gvfunc(ptr, keylength);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				break;
			}
			assert(keylength < len - 1);
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			src.len = len - keylength - 1;
			src.addr = (char *)(ptr + keylength + 1);
			des.len = 0;
			if (src.len > max_record_size)
			{
			        max_record_size = src.len;
			        free(rec_buff);
				rec_buff = (unsigned char *)malloc(max_record_size);
			}
			des.addr = (char *)rec_buff;
			if (FALSE == zwr2format(&src, &des))
                        {
                                util_out_print("Format error in record !8UL: !/!AD", TRUE, rec_count, src.len, src.addr);
				format_error = TRUE;
				continue;
                        }
			if (max_data_len < des.len)
			        max_data_len = des.len;
			mupip_put_gvdata(rec_buff, des.len);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				break;
			}
			key_count++;
		} else
		{
			gv_currkey->end = 0;
		        str2gvkey_gvfunc(ptr, len);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				break;
			}
			if (max_subsc_len < (gv_currkey->end + 1))
				max_subsc_len = gv_currkey->end + 1;
			rec_count++;
			if (rec_count > end)
			{
			        rec_count--;	/* Decrement as didn't load key */
			        break;
			}
			if (RMS$_EOF == (status = sys$get(inrab)))
			        break;
			if (RMS$_NORMAL != status)
			{
			        mupip_error_occurred = TRUE;
				mu_gvis();
				rts_error(VARLSTCNT(14) ERR_LOADFILERR, 2, infab->fab$b_fns, infab->fab$l_fna,
						ERR_SYSCALL, 5, LEN_AND_LIT("SYS$GET"), CALLFROM, status, 0, inrab->rab$l_stv);
				break;
			}
			len = inrab->rab$w_rsz;
			ptr = inrab->rab$l_rbf;
			while (0 < len && ((ASCII_LF == *(ptr + len - 1)) || (ASCII_CR == *(ptr + len - 1))))
			        len--;
			if (max_data_len < len)
			        max_data_len = len;
			mupip_put_gvdata(ptr, len);
			if (mupip_error_occurred)
			{
			        mu_gvis();
				break;
			}
			key_count++;
		}
	}
	gv_cur_region = NULL;
	REVERT;
	status = sys$close(infab);
	if (RMS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(14) ERR_LOADFILERR, 2, infab->fab$b_fns, infab->fab$l_fna,
				ERR_SYSCALL, 5, LEN_AND_LIT("SYS$CLOSE"), CALLFROM, status, 0, inrab->rab$l_stv);
		mupip_error_occurred = TRUE;
	}
	if (mupip_error_occurred)
	{
		gtm_putmsg(VARLSTCNT(3) ERR_LOADABORT, 1, rec_count);
		mupip_exit( ERR_MUNOACTION );
	}
	if (mu_ctrly_occurred)
		rts_error(VARLSTCNT(1) ERR_LOADCTRLY);
	mu_load_stat(max_data_len, max_subsc_len, key_count, key_count ? (rec_count - 1) : 0, ERR_RECCNT);
	free(inrab->rab$l_ubf);
	free(rec_buff);
	if (format_error)
		mupip_exit(ERR_MUNOFINISH);
}
