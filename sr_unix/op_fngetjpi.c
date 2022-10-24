/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

GBLREF spdesc 	stringpool;
GBLREF uint4	process_id;

typedef char	keyword[MAX_KEY];

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

	if (!key[keywd_indx][0])
                 rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADJPIPARAM, 2, kwd->str.len, kwd->str.addr);
	if (0 == jpid)
	{	/* Treat 0 as the current process id for backward compatibility reasons (YDB#908) */
		jpid = process_id;
	}
	if (kw_isprocalive == keywd_indx)
	{
		info = (process_id != jpid) ? is_proc_alive(jpid, 0) : 1;
		ui82mval(ret, info);
	} else
	{	/* It is a time keyword. Get the time values from the /proc/<pid>/stat file.
		 * See /proc/[pid]/stat section in https://man7.org/linux/man-pages/man5/proc.5.html
		 * for details on the format of the file.
		 *
		 * Below is a list of the first 17 fields
		 *	Field  1 : pid %d
		 *	Field  2 : comm %s
		 *	Field  3 : state %c
		 *	Field  4 : ppid %d
		 *	Field  5 : pgrp %d
		 *	Field  6 : session %d
		 *	Field  7 : tty_nr %d
		 *	Field  8 : tpgid %d
		 *	Field  9 : flags %u
		 *	Field 10 : minflt %lu
		 *	Field 11 : cminflt %lu
		 *	Field 12 : majflt %lu
		 *	Field 13 : cmajflt %lu
		 *	Field 14 : utime %lu
		 *	Field 15 : stime %lu
		 *	Field 16 : cutime %ld
		 *	Field 17 : cstime %ld
		 *
		 * The original implementation of reading this file used "fscanf()" to skip the first 13 fields and then read
		 * only fields 14 thru 17. Something like the following.
		 *
		 *	fscanf_ret = FSCANF(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %ld %ld",
		 *			&field14, &field15, &field16, &field17);
		 *
		 * But that did not work with the "comm" field (Field 2) as it contained spaces in some
		 * cases and a simple "%*s" did not work. It caused later fields (which is what we care about)
		 * to be misinterpreted.
		 *
		 * This is a known issue with parsing /proc/stat using C as mentioned at https://stackoverflow.com/a/66359428.
		 *
		 * --------- Paste BEGIN ------
		 * The biggest issue is usually the "comm" field (The second field in the file). According to the man pages, this
		 * field is a string that should be "scanned" using some scanf flavor and the formatter "%s". But that is wrong!
		 *
		 * The "comm" field is controlled by the application (Can be set using prctl(PR_SET_NAME, ...)) and can easily
		 * include spaces or brackets, easily causing 99% of the parsers out there to fail. And a simple change like
		 * that won't just return a bad comm value, it will screw up with all the values that come after it.
		 * --------- Paste END  ------
		 *
		 * And true enough, I did see file names that included spaces and names that even excluded the 16-byte length
		 * that is in the documentation as the maximum length of the "comm" field. Examples of such names are
		 *
		 * a) (kworker/13:0H-events_highpri)
		 * b) (NFSv4 callback)
		 *
		 * The same url lists 2 approaches to parse this file. Out of which the first approach made sense to me.
		 * That is pasted below.
		 *
		 * --------- Paste BEGIN ------
		 * The right way to parse the file are one of the following:
		 *
		 * Option #1:
		 * 1) Read the entire content of the file
		 * 2) Find the first occurrence of '('
		 * 3) Find the last occurrence of ')'
		 * 4) Assign to comm the string between those indices
		 * 5) Parse the rest of the file after the last occurrence of ')'
		 * --------- Paste END  ------
		 *
		 * And so that is the approach chosen here.
		 */
		char		filename[64];	/* The file name is "/proc/PID/stat". Since PID is at most a 20 byte number
						 * 64 bytes is much more than enough to store this name.
						 */
		FILE		*fp;
		int		sscanf_ret, status;
		/* And below is a list of the 4 fields (fields 14 thru 17) that are not skipped in the "FSCANF" call below */
		unsigned long	field14;	/* utime %lu */
		unsigned long	field15;	/* stime %lu */
		long		field16;	/* cutime %ld */
		long		field17;	/* cstime %ld */

		sprintf(filename, "/proc/%d/stat", jpid);
		Fopen(fp, filename, "r");
		if (NULL != fp)
		{
			off_t	file_offset;
			size_t	read_size, ret_size;
			char	file_contents[1024];	/* The format of the /proc/PID/stat file has only one string and
							 * lots of int/long numbers and so the offset of the file where
							 * field17 ends should be almost always less than 100 bytes but
							 * we allocate a 1024 byte buffer just in case. Also see later
							 * comment about 1024 below.
							 */
			int	save_errno;
			char	*last_right_paren;
			char	*ptr, *ptr_top;

			/* Note: I originally thought of an approach where we determined the file size and allocated a
			 * buffer of that size and read the entire file contents into that buffer and then parse it.
			 * But that did not work because "fseek", "ftell" etc. do not work on "/proc/PID" files.
			 * Hence decided to limit the buffer size to 1024 and read just that much out of the file.
			 */
			read_size = SIZEOF(file_contents);
			GTM_FREAD(file_contents, 1, read_size, fp, ret_size, save_errno);
			if ((ret_size < read_size) && save_errno)
			{
				char	errstr[128];

				SNPRINTF(errstr, SIZEOF(errstr), "fread() : %s : Expected = %lld : Actual = %lld",
								fp, (long long)read_size, (long long)ret_size);
				/* ERROR encountered during GTM_FREAD */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
						ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
			}
			/* Find the last ")" in the buffer. We can use SSCANF after that point onwards without any issues. */
			last_right_paren = NULL;
			for (ptr = file_contents, ptr_top = file_contents + ret_size; ptr < ptr_top; ptr++)
				if (')' == *ptr)
					last_right_paren = ptr;
			assert(NULL != last_right_paren);
			if (NULL == last_right_paren)
				ptr = file_contents;
			else
				ptr = last_right_paren + 2;	/* skip ')' and ' ' after the "comm" field */
			sscanf_ret = SSCANF(ptr, "%*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %ld %ld",
					&field14, &field15, &field16, &field17);
			UNUSED(sscanf_ret);
			assert(EOF != sscanf_ret);
			FCLOSE(fp, status);
			switch (keywd_indx)
			{
				case kw_utime:
					info = field14;
					break;
				case kw_stime:
					info = field15;
					break;
				case kw_cutime:
					info = field16;
					break;
				case kw_cstime:
					info = field17;
					break;
				case kw_cputim:
					info = field14 + field15 + field16 + field17;
					break;
				case kw_end:
				default:
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADJPIPARAM, 2, kwd->str.len, kwd->str.addr);
					return;
			}
			SYSCONF(_SC_CLK_TCK, sc_clk_tck);
			GTM_WHITE_BOX_TEST(WBTEST_FAKE_BIG_CNTS, info, info << 31);
			info = (gtm_uint64_t)((info * 100) / sc_clk_tck);	/* Convert to standard 100 ticks per second */
			ui82mval(ret, info);
		} else
		{	/* File open failed. Most likely due to pid not existing at this point. Return -1 in this case. */
			i2mval(ret, -1);
		}
	}
}
