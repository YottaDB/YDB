/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_fcntl.h"

#include <ssdef.h>
#include <rms.h>
#include <devdef.h>
#include <descrip.h>
#include <libdtdef.h>
#include <libdef.h>
#include <starlet.h>

#include "iosp.h"
#include "gtm_file_stat.h"
#include "gtm_rename.h"

#define	YR_DIGIT_SIZE	4
#define	TIME_DIGIT_SIZE	6

/* This appends timestamp from file (fn) last modified status time. Result is returned in same string fn.
 * Return SS_NORMAL for success */
uint4 append_time_stamp(char *fn, int fn_len, int *app_len, uint4 *ustatus)
{
	struct FAB	fab;
	struct NAM	nam;
	struct XABDAT	xabdat;
	gtm_int64_t 	*cdt;
	int		stat_res;
	uint4		status1;
	char		es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN];
	char		yr_arr[MAX_FN_LEN], yr_time_arr[MAX_FN_LEN], time_arr[MAX_FN_LEN];
	char 		format_arr[MAX_FN_LEN], output_arr[MAX_FN_LEN];
	short 		yr_len;
	unsigned long	days, context = 0;
	long yearflag = LIB$K_OUTPUT_FORMAT;
	long daysflag = LIB$K_DAY_OF_YEAR;
	$DESCRIPTOR(yr_time_format,"|!Y4|!H04!M0!S0|");
	$DESCRIPTOR(yr_time_str, yr_time_arr);

	*ustatus = SS_NORMAL;
	nam = cc$rms_nam;
	xabdat = cc$rms_xabdat;
	cdt = &(xabdat.xab$q_cdt);	/* Creation Date and Time */
	nam.nam$l_rsa = name_buffer;
	nam.nam$b_rss = SIZEOF(name_buffer);
	nam.nam$l_esa = es_buffer;
	nam.nam$b_ess = SIZEOF(es_buffer);
	fab = cc$rms_fab;
	fab.fab$l_xab = &xabdat;
	fab.fab$l_nam = &nam;
	fab.fab$b_fac = FAB$M_BIO | FAB$M_GET;
	fab.fab$l_fop = FAB$M_UFO;
	fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_SHRDEL | FAB$M_SHRUPD | FAB$M_UPI;
	fab.fab$l_fna = fn;
	fab.fab$b_fns = fn_len;
	status1 = sys$open(&fab);	/* Open to get the latest modified time */
	if (0 == (status1 & 1))
	{
		*ustatus = fab.fab$l_stv;
		return status1;
	}
	sys$dassgn(fab.fab$l_stv); /* Got date so no longer need file open */
	if (LIB$_NORMAL != (status1 = lib$cvt_from_internal_time(&daysflag, &days, cdt))) /* Get julian date (day of year) */
		return status1;
	if (SS$_NORMAL != (status1 = lib$init_date_time_context(&context, &yearflag, &yr_time_format)))
		return status1;
	yr_len = MAX_FN_LEN;
	if (SS$_NORMAL != (status1 = lib$format_date_time(&yr_time_str, cdt, &context, &yr_len, 0))) /* get the year string */
	{
		lib$free_date_time_context(&context);
		return status1;
	}
	if (SS$_NORMAL != (status1 = lib$free_date_time_context(&context)))
		return status1;
	yr_time_arr[yr_len]='\0';
	memcpy(yr_arr, yr_time_arr, YR_DIGIT_SIZE);
	yr_arr[YR_DIGIT_SIZE] = '\0';
	memcpy(time_arr, &yr_time_arr[YR_DIGIT_SIZE + 1], TIME_DIGIT_SIZE);
	time_arr[TIME_DIGIT_SIZE] = '\0';
	SPRINTF(fn + fn_len, "_%s%d%s", yr_arr, days, time_arr);
	*app_len = strlen(fn) - fn_len;
	return SS_NORMAL;
}

