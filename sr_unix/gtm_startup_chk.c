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

/************************************************************************************
  *	Current GT.M operation on Unix depends on $gtm_dist environment variable
  *	being set correctly or it cannot communicate with gtmsecshr. If this
  *	communication fails, it can cause many problems.
  *	This is the startup checking including the following:
  *	gtm_chk_dist()
  *		1. $gtm_dist is defined
  *		2. $gtm_dist need to point to the current directory from which
  *			mumps and mupip were being executed from. This is verified
  *			in gtm_chk_dist() to avoid gtmsecshr forking errors later on.
  *			see C9808-000615
  *		3. $gtm_dist/gtmsecshr has setuid privs and is owned by root
  *	gtm_chk_image()
  *		4. the current image is $gtm_dist/mumps
  *	If either of these are not true, it exits.
  ************************************************************************************/
#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "gdsroot.h"
#include "error.h"
#include "parse_file.h"
#include "is_file_identical.h"
#include "gtm_startup_chk.h"

#define	GTM_DIST		"gtm_dist"
#define GTM_IMAGE		"mumps"
#define GTM_IMAGE_NAMELEN	(sizeof(GTM_IMAGE) - 1)
#define	DIR_SEPARATOR		'/'
#define	GTMSECSHR_NAME		"gtmsecshr"
#define GTMSECSHR_NAMELEN	(sizeof(GTMSECSHR_NAME) - 1)
#define	ROOTUID			0	/*	also defined in gtmsecshr.h	*/

static char		pbuff[MAX_FBUFF];
static int		plen = 0;
static char 		mbuff[MAX_FBUFF];
static parse_blk	pblk;

int gtm_chk_dist(char *image)
{
	char		*ptr1;
	char		pre_buff[MAX_FBUFF];
	char		*prefix;
	int		prefix_len;
	mstr		gtm_name;
	int		status;
	char		save_ch = DIR_SEPARATOR;

	error_def(ERR_DISTPATHMAX);
	error_def(ERR_GTM_DIST);
	error_def(ERR_SYSCALL);
	error_def(ERR_GTMDISTUNDEF);
	error_def(ERR_FILEPARSE);
	error_def(ERR_MAXGTMPATH);

	memset(pbuff, 0, MAX_FBUFF);
	memset(&pblk, 0, sizeof(pblk));
	if (NULL != (ptr1 = (char *)GETENV(GTM_DIST)))
	{
		plen = strlen(ptr1);
		if (MAX_FBUFF - 2 > plen + GTMSECSHR_NAMELEN)
			memcpy(pbuff, ptr1, plen);
		else
			rts_error(VARLSTCNT(3) ERR_DISTPATHMAX, 1, MAX_FBUFF - GTMSECSHR_NAMELEN);
	}
	else
		rts_error(VARLSTCNT(1) ERR_GTMDISTUNDEF);
	if (DIR_SEPARATOR != pbuff[plen - 1])
	{
		pbuff[plen] = DIR_SEPARATOR;
		pbuff[plen + 1] = 0;
		plen++;
	}

	pblk.buffer = mbuff;
	pblk.buff_size = MAX_FBUFF;
	pblk.fop = F_SYNTAXO;
	gtm_name.addr = image;
	gtm_name.len = strlen(image);
	/*	strings returned in pblk are not null terminated 	*/
	status = parse_file(&gtm_name, &pblk);
	if (!(status & 1))
		rts_error(VARLSTCNT(5) ERR_FILEPARSE, 2, gtm_name.len, gtm_name.addr, status);
	assert(NULL != pblk.l_name);
	if ((!(pblk.fnb & F_HAS_DIR) && !pblk.b_dir) || (DIR_SEPARATOR != pblk.l_dir[0]))
	{
		if (NULL == GETCWD(pre_buff, MAX_FBUFF, prefix))
			rts_error(VARLSTCNT(7) ERR_SYSCALL, 5,
					LEN_AND_LIT("getcwd"), CALLFROM);

		prefix_len = strlen(prefix);
		if(MAX_FBUFF < prefix_len + pblk.b_esl + 1)
			rts_error(VARLSTCNT(3) ERR_MAXGTMPATH, 1, MAX_FBUFF - pblk.b_name);
		if (DIR_SEPARATOR != prefix[prefix_len - 1])
		{
			prefix[prefix_len] = DIR_SEPARATOR;
			prefix[++prefix_len] = 0;
		}
		memcpy(prefix + prefix_len, pblk.l_dir, (int)pblk.b_dir);
		prefix[prefix_len + (int)pblk.b_dir] = 0;
	}
	else
	{
		prefix = pblk.l_dir;
	/*
	 *	save the char in the parsed buffer, this is the first char
	 *	of the file(image) name, restore it after strcmp below;
	 *	gtm_chk_image might use it
	 */
		save_ch = prefix[pblk.b_dir];
		prefix[pblk.b_dir] = 0;
	}
	if (!is_file_identical(prefix, pbuff))
	{
		if (DIR_SEPARATOR != save_ch)
			prefix[pblk.b_dir] = save_ch;
		rts_error(VARLSTCNT(4) ERR_GTM_DIST, 2, pblk.b_name, pblk.l_name);
	}
	if (DIR_SEPARATOR != save_ch)
		prefix[pblk.b_dir] = save_ch;

	return 0;
}

int gtm_chk_image(void)
{
	error_def(ERR_IMAGENAME);

	if (memcmp(pblk.l_name, GTM_IMAGE, GTM_IMAGE_NAMELEN))
		rts_error(VARLSTCNT(6) ERR_IMAGENAME, 4, LEN_AND_LIT(GTM_IMAGE), pblk.b_name, pblk.l_name);

	return 0;
}
