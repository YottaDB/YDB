/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"

#define	YR_DIGIT_SIZE	4
#define	TIME_DIGIT_SIZE	6

/* Append the formatted timestamp to the file name (fn); *fn_len contains the current length of the filename and at exit from this
 * function, it is updated to reflect the new length.
 */
uint4 append_time_stamp(char *fn, int *fn_len, jnl_tm_t now)
{
	uint4		status1;
	gtm_uint64_t	whole_time;
	char		es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN];
	char		yr_arr[MAX_FN_LEN], yr_time_arr[MAX_FN_LEN], time_arr[MAX_FN_LEN];
	char 		format_arr[MAX_FN_LEN], output_arr[MAX_FN_LEN];
	short 		yr_len;
	unsigned long	days, context = 0;
	long yearflag = LIB$K_OUTPUT_FORMAT;
	long daysflag = LIB$K_DAY_OF_YEAR;
	$DESCRIPTOR(yr_time_format, JNLSWITCH_TM_FMT);
	$DESCRIPTOR(yr_time_str, yr_time_arr);

	JNL_WHOLE_FROM_SHORT_TIME(whole_time, now);
	if (LIB$_NORMAL != (status1 = lib$cvt_from_internal_time(&daysflag, &days, &whole_time))) /* Get julian date */
		return status1;
	if (SS$_NORMAL != (status1 = lib$init_date_time_context(&context, &yearflag, &yr_time_format)))
		return status1;
	yr_len = MAX_FN_LEN;
	if (SS$_NORMAL != (status1 = lib$format_date_time(&yr_time_str, &whole_time, &context, &yr_len, 0))) /* get the year */
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
	SPRINTF(fn + *fn_len, "_%s%d%s", yr_arr, days, time_arr);
	*fn_len = strlen(fn);
	return SS_NORMAL;
}
