/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <stddef.h>
#include <errno.h>

#include "mdef.h"
#include "gtm_limits.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "repl_msg.h"
#include "repl_log.h"
#include "gdsroot.h"
#include "have_crit.h"
#include "min_max.h"

error_def(ERR_FILENAMETOOLONG);

GBLREF uint4	process_id;

uint4	repl_logfileinfo_get(char *logfile, repl_logfile_info_msg_t *msgp, boolean_t cross_endian, FILE *logfp)
{
	uint4		status, fullpath_len, msglen;
	int		save_errno;
	char		fullpath[GTM_PATH_MAX], *cwdptr;

	assert(NULL != msgp);
	msgp->type = cross_endian ? GTM_BYTESWAP_32(REPL_LOGFILE_INFO) : REPL_LOGFILE_INFO;
	assert(GTM_PATH_MAX >= REPL_LOGFILE_PATH_MAX);
	assert(GTM_PATH_MAX >= PATH_MAX);
	if (NULL == logfile)
	{
		GETCWD(fullpath, GTM_PATH_MAX, cwdptr);
		assert(NULL != cwdptr);
		if (NULL == cwdptr)
		{
			save_errno = errno;
			assert(FALSE);
			repl_log(logfp, TRUE, TRUE, "Could not obtain current working directory: %s\n", STRERROR(save_errno));
			SNPRINTF(fullpath, GTM_PATH_MAX, "Could not obtain current working directory");
		}
		fullpath_len = STRLEN(fullpath);
	} else if (!get_full_path(STR_AND_LEN(logfile), fullpath, &fullpath_len, GTM_PATH_MAX + 1, &status))
	{	/* Either GETCWD failed or buffer not large enough to hold the expanded logfile path. In either case, we don't want
		 * to error out as this is just a supplementary message. Copy whatever possible.
		 */
		assert(ERR_FILENAMETOOLONG != status);
		SNPRINTF(fullpath, GTM_PATH_MAX, logfile);
		fullpath_len = STRLEN(fullpath);
		/* Print a warning message for diagnostic purposes */
		if (ERR_FILENAMETOOLONG != status)
			repl_log(logfp, TRUE, TRUE, "Could not obtain current working directory: %s\n", STRERROR(status));
		else
			repl_log(logfp, TRUE, TRUE, "Could not obtain full path of log file: Path name exceeds %d characters\n",
					GTM_PATH_MAX);
	}
	assert('\0' == fullpath[fullpath_len]);
	fullpath_len = MIN(fullpath_len, REPL_LOGFILE_PATH_MAX);
	fullpath[fullpath_len] = '\0';	/* truncate if needed */
	fullpath_len++;			/* So that, we copy and send null-terminator as well */
	memcpy(msgp->fullpath, fullpath, fullpath_len);
	msgp->fullpath_len = cross_endian ? GTM_BYTESWAP_32(fullpath_len) : fullpath_len;
	assert(fullpath_len <= REPL_LOGFILE_PATH_MAX);
	/* Receiver expects 8 byte alignment on data portion of the message. */
	fullpath_len = ROUND_UP2(fullpath_len, REPL_MSG_ALIGN);
	assert(fullpath_len <= REPL_LOGFILE_PATH_MAX + 1);
	msglen = REPL_LOGFILE_INFO_MSGHDR_SZ + fullpath_len;
	msgp->len = cross_endian ? GTM_BYTESWAP_32(msglen) : msglen;
	msgp->proto_ver = REPL_PROTO_VER_THIS;
	msgp->pid = cross_endian ? GTM_BYTESWAP_32(process_id) : process_id;
	return msglen;
}
