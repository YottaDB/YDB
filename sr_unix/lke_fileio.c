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

/*  lke_fileio.c  */


#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_unistd.h"

#include "error.h"
#include "cli.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "lke_fileio.h"
#include "gtmio.h"
#ifdef __MVS__
#include "gtm_stat.h"
#include "gtm_zos_io.h"
#endif

/* This function will process I/O if -OUTPUT suboption is present.
   This will create the file and duplicate it as stderr.
   So anything written to stderr will be written to this file. */
bool open_fileio(int *save_stderr)
{
	char		ofnamebuf[1024];
	mstr		ofname;
	int		status=FALSE, fd;
	unsigned short	len;
	char		*errmsg;
#ifdef __MVS__
	int		realfiletag, fstatus;
	struct stat	info;
	/* Need the ERR_BADTAG and ERR_TEXT  error_defs for the TAG_POLICY macro warning */
	error_def(ERR_TEXT);
	error_def(ERR_BADTAG);
#endif

	*save_stderr = SYS_STDERR;
	ofname.addr=ofnamebuf;
	ofname.len=SIZEOF(ofnamebuf);
	if (cli_present("OUTPUT") == CLI_PRESENT)
	{
		len = ofname.len;
	 	if (cli_get_str("OUTPUT", ofname.addr, &len))
		{
			int dup2_res;
			ZOS_ONLY(STAT_FILE(ofname.addr, &info, fstatus);)
			/* create output file */
	 		CREATE_FILE(ofname.addr, 0666, fd);
			if (0 > fd)
			{
				errmsg = STRERROR(errno);
				util_out_print("Cannot create !AD.!/!AD", TRUE, len, ofname.addr,
					RTS_ERROR_STRING(errmsg));
				return status;
			}
			/* All output from LKE is done through util_out_print which uses stderr so make output file as new stderr */
			*save_stderr = dup(SYS_STDERR);
			DUP2(fd, SYS_STDERR, dup2_res);
#ifdef __MVS__
			if (0 == fstatus)
			{
				if (-1 == gtm_zos_tag_to_policy(fd, TAG_UNTAGGED, &realfiletag))
					TAG_POLICY_GTM_PUTMSG(ofname.addr, realfiletag, TAG_UNTAGGED, errno);
			} else
			{
				if (-1 == gtm_zos_set_tag(fd, TAG_EBCDIC, TAG_TEXT, TAG_FORCE, &realfiletag))
					TAG_POLICY_GTM_PUTMSG(ofname.addr, realfiletag, TAG_EBCDIC, errno);
			}
#endif
			status=TRUE;
		} else
			util_out_print("Error getting FILE name", TRUE);
	}
	return status;
}

/* Close the file I/O and restore stderr */
void close_fileio(int *save_stderr)
{
	int	dup2_res;
	int	rc;

	DUP2(*save_stderr, SYS_STDERR, dup2_res);
	CLOSEFILE_RESET(*save_stderr, rc);	/* resets "*save_stderr" to FD_INVALID */
	*save_stderr = SYS_STDERR;
}
