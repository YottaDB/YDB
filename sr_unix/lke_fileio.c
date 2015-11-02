/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

/* This function will process I/O if -OUTPUT suboption is present.
   This will create the file and duplicate it as stderr.
   So anything written to stderr will be written to this file. */
bool open_fileio(int *save_stderr)
{
	char		ofnamebuf[1024];
	mstr		ofname;
	int		status=FALSE, fd;
	unsigned short	len;

	*save_stderr = 2;
	ofname.addr=ofnamebuf;
	ofname.len=sizeof(ofnamebuf);
	if (cli_present("OUTPUT") == CLI_PRESENT)
	{
		len = ofname.len;
	 	if (cli_get_str("OUTPUT", ofname.addr, &len))
		{
			int dup2_res;
			/* create output file */
			CREATE_FILE(ofname.addr, 0666, fd);
			if (0 > fd)
			{
				util_out_print("Cannot create !AD.!/!AD", TRUE, len, ofname.addr,
					RTS_ERROR_STRING(STRERROR(errno)));
				return status;
			}
			/* All output from LKE is done through util_out_print which uses stderr so make output file as new stderr */
			*save_stderr = dup(2);
			DUP2(fd, 2, dup2_res);
			status=TRUE;
		} else
			util_out_print("Error getting FILE name", TRUE);
	}
	return status;
}

/* Close the file I/O and restore stderr */
void close_fileio(int save_stderr)
{
	int dup2_res;

	DUP2(save_stderr, 2, dup2_res);
	close(save_stderr);
}
