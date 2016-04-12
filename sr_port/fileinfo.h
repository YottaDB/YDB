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

#ifndef __FILE_INFO_H__
#define __FILE_INFO_H__

#define FI_USR_SZ 31
#define FI_TRM_SZ 7

typedef struct
{
	gtm_facility		fac;			/* facility */
	short			dat[4];			/* date (quadword) */
	char			usr[FI_USR_SZ];		/* user name */
	char			trm[FI_TRM_SZ];		/* terminal identification */
	char			filler[2];		/* used for longword alignment */
}file_info;

#define FI_NUM_ENT 5
typedef struct
{
	int4		cnt;			/* number of entries inserted into ent.
						 * ent is a circular queue so
						 *	ent[ cnt % FI_NUM_ENT]
						 * is always the next location to insert.
						 */
	file_info	ent[FI_NUM_ENT];	/* entries */
}file_log;

#endif
