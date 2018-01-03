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

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_string.h"

#include "stringpool.h"
#include "gtm_times.h"
#include "gtm_caseconv.h"
#include "op.h"
#include "mvalconv.h"
#include "is_proc_alive.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "have_crit.h"

#define MAX_KEY 16
#define MAX_STR 16

GBLREF spdesc 	stringpool ;

typedef char	keyword[MAX_KEY] ;

#define	MAX_KEY_LEN	20	/* maximum length across all keywords in the key[] array below */

error_def	(ERR_BADJPIPARAM);
error_def	(ERR_SYSCALL);

static keyword	key[]= {
	"CPUTIM",
	"CSTIME",
	"CUTIME",
	"ISPROCALIVE",
	"STIME",
	"UTIME",
	""
} ;

enum 	kwind {
	kw_cputim,
	kw_cstime,
	kw_cutime,
	kw_isprocalive,
	kw_stime,
	kw_utime,
	kw_end
};

void op_fngetjpi(mint jpid, mval *kwd, mval *ret)
{
	struct tms	proc_times;
	gtm_uint64_t	info, sc_clk_tck;
	int		keywd_indx;
	char		upcase[MAX_KEY_LEN];

	assert (stringpool.free >= stringpool.base);
	assert (stringpool.top >= stringpool.free);
	ENSURE_STP_FREE_SPACE(MAX_STR);

	MV_FORCE_STR(kwd);
	if (kwd->str.len == 0)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADJPIPARAM, 2, 4, "Null");

	if (MAX_KEY < kwd->str.len)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADJPIPARAM, 2, kwd->str.len, kwd->str.addr);

	lower_to_upper((uchar_ptr_t)upcase, (uchar_ptr_t)kwd->str.addr, (int)kwd->str.len);

	keywd_indx = kw_cputim ;
	/* future enhancement:
	 * 	(i) since keywords are sorted, we can exit the while loop if 0 < memcmp.
	 * 	(ii) also, the current comparison relies on kwd->str.len which means a C would imply CPUTIM instead of CSTIME
	 * 		or CUTIME this ambiguity should probably be removed by asking for an exact match of the full keyword
	 */
	while (key[keywd_indx][0] && (0 != STRNCMP_STR(upcase, key[keywd_indx], kwd->str.len)))
		keywd_indx++;

	if( !key[keywd_indx][0] )
        {
                 rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADJPIPARAM, 2, kwd->str.len, kwd->str.addr);
        }

	if ((kw_isprocalive != keywd_indx) && ((unsigned int)-1 == times(&proc_times)))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("clock_gettime"), CALLFROM, errno);
		return;
	}
	switch (keywd_indx)
	{
		case kw_cputim:
			info = proc_times.tms_utime + proc_times.tms_stime + proc_times.tms_cutime + proc_times.tms_cstime;
			break;
		case kw_cstime:
			info = proc_times.tms_cstime;
			break;
		case kw_cutime:
			info = proc_times.tms_cutime;
			break;
		case kw_isprocalive:
			info = (0 != jpid) ? is_proc_alive(jpid, 0) : 1;
			break;
		case kw_stime:
			info = proc_times.tms_stime;
			break;
		case kw_utime:
			info = proc_times.tms_utime;
			break;
		case kw_end:
		default:
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADJPIPARAM, 2, kwd->str.len, kwd->str.addr);
			return;
	}
	if (kw_isprocalive != keywd_indx)
	{
		SYSCONF(_SC_CLK_TCK, sc_clk_tck);
		GTM_WHITE_BOX_TEST(WBTEST_FAKE_BIG_CNTS, info, info << 31);
		info = (gtm_uint64_t)((info * 100) / sc_clk_tck);	/* Convert to standard 100 ticks per second */
	}
	ui82mval(ret, info);
}
