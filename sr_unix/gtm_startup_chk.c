/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/************************************************************************************
  *	Current GT.M operation on Unix depends on $gtm_dist environment variable
  *	being set correctly or it cannot communicate with gtmsecshr. If this
  *	communication fails, it can cause many problems.
  *	This is the startup checking including the following:
  *	gtm_chk_dist()
  *
  *	1. $gtm_dist is defined
  *
  *	2. Ensure that the executable resides in $gtm_dist. This provides enough
  *	assurance that $gtm_dist is suitable for use.
  *
  *	This comparison is done using the inode of the executable in $gtm_dist
  *	and the discovered executable path, either from /proc or other OS
  *	function. Using argv[0] wasn't possible when $gtm_dist is in the $PATH.
  *
  *	The GT.CM servers, OMI and GNP, always defer issuing an error for an
  *	unverified $gtm_dist. All other executables, MUMPS, MUPIP, DSE, and LKE
  *	issue the error messages as soon as possible.
  *
  *	3. Checking that $gtm_dist/gtmsecshr was relocated to secshr_client()
  ************************************************************************************/
#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "gtm_limits.h"
#include "gdsroot.h"
#include "error.h"
#include "parse_file.h"
#include "is_file_identical.h"
#include "gtm_startup_chk.h"
#include "gtmimagename.h"
#include "have_crit.h"

/* HPUX does not have a /proc filesystem to retrieve the exe's path. The
 * following link leads to a forum thread in which the user yrpark shows the
 * desired result.
 * http://h30499.www3.hp.com/t5/Languages-and-Scripting/Get-full-path-of-executable-of-running-process/td-p/5135520#.U-o1W3X7GV4
 */
#if defined(__hpux)
#include <sys/pstat.h>
#endif

GBLREF	char		gtm_dist[GTM_PATH_MAX];
GBLREF	boolean_t	gtm_dist_ok_to_use;
LITREF	gtmImageName	gtmImageNames[];
GBLREF	uint4		process_id;

error_def(ERR_DISTPATHMAX);
error_def(ERR_SYSCALL);
error_def(ERR_GTMDISTUNDEF);
error_def(ERR_GTMDISTUNVERIF);
error_def(ERR_FILEPARSE);
error_def(ERR_MAXGTMPATH);
error_def(ERR_IMAGENAME);
error_def(ERR_TEXT);

int gtm_chk_dist(char *image)
{
	char		*real_dist;
	char		*imagepath;
	char		*exename;
	int		exename_len;
	int		path_len;
	int		gtm_dist_len;
	boolean_t	status;
	boolean_t	is_gtcm_image;
	char		image_real_path[GTM_PATH_MAX];
	char		real_gtm_dist_path[GTM_PATH_MAX];
	char		comparison[GTM_PATH_MAX];

	is_gtcm_image = (IS_GTCM_GNP_SERVER_IMAGE || IS_GTCM_SERVER_IMAGE); /* GT.CM servers defer issuing errors until startup */
	/* Use the real path while checking the path length. If not valid, let it fail when checking is_file_identical  */
	real_dist = realpath(gtm_dist, real_gtm_dist_path);
	if (real_dist)
	{
		STRNLEN(real_dist, GTM_PATH_MAX, gtm_dist_len);
	} else
	{
		STRNLEN(gtm_dist, GTM_PATH_MAX, gtm_dist_len);
	}
	if (gtm_dist_len)
	{
		assert(IS_VALID_IMAGE && (n_image_types > image_type));	/* assert image_type is initialized */
		if (GTM_DIST_PATH_MAX <= gtm_dist_len)
		{
			if (is_gtcm_image)
				return 0;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_DISTPATHMAX, 1, GTM_DIST_PATH_MAX);
		}
	} else
	{
		if (is_gtcm_image)
			return 0;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_GTMDISTUNDEF);
	}

	/* Get the executable name and length */
	exename = strrchr(image, '/');
	if (!exename) /* no slash found, then image is just the exe's name */
	{
		exename = image;
	} else /* slash found in the path, advance the pointer by one to get the name */
		exename++;
	STRNLEN(exename, GTM_PATH_MAX, exename_len);

	status = gtm_image_path(image_real_path);
	if (status) /* On HP-UX it's possible for the underlying system call to return an error. Fall back to realpath() */
	{
		/* Use the return of realpath(). If it returns null for argv[0], just use image and let the check fail */
		imagepath = realpath(image, image_real_path);
		if (NULL == imagepath)
			imagepath = image;
	} else
		imagepath = image_real_path;
	/* create the comparison path (gtm_dist + '/' + exename + '\0') and compare it to imagepath */
	SNPRINTF(comparison, GTM_PATH_MAX, "%s/%s", gtm_dist, exename);
	status = is_file_identical(imagepath, comparison);
	if (status)
		gtm_dist_ok_to_use = TRUE;
	else
	{
		if (is_gtcm_image)
			return 0;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_GTMDISTUNVERIF, 4, LEN_AND_STR(gtm_dist), LEN_AND_STR(image));
	}

	if (IS_GTM_IMAGE && memcmp(exename, GTM_IMAGE_NAME, GTM_IMAGE_NAMELEN))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_IMAGENAME, 4, LEN_AND_LIT(GTM_IMAGE_NAME), LEN_AND_STR(exename));
	return 0;
}

int gtm_image_path(char *realpath)
{
#if defined(__hpux)
	int		  status = 0;
	struct pst_status pst;
	pst.pst_pid = -1;
	/* The man page for pstat_getproc does not list in/valid return codes, so we ignore it */
	pstat_getproc(&pst, SIZEOF(struct pst_status), 0, process_id);
	status = pstat_getpathname(realpath, GTM_PATH_MAX, &pst.pst_fid_text);
	assertpro(status != 0); /* Can only happen if the path name is not in the system cache */
	if (status < 0) /* errno is set */
		return status;
#elif defined(__linux__) || defined(__sparc) ||  defined(_AIX) || defined(__CYGWIN__)
	SNPRINTF(realpath, GTM_PATH_MAX, PROCSELF, process_id);
#else
#	error "Unsupported platform : no way to determine the true exe path"
#endif
	return 0;
}
