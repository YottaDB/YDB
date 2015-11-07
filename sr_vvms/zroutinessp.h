/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __ZROUTINESSP_H__
#define __ZROUTINESSP_H__

#include <nam.h>
/*
 * CAUTION ----	It is assumed that dvi, fid and did are contiguous not
 *		only in the zro_ent structures but also in the RMS
 *		NAM structure.
 */

typedef	struct zro_ent_type
{
	uint4		type;
	uint4		count;
	boolean_t	node_present;
	mstr		str;
	char		dvi[NAM$C_DVI];
	unsigned short	fid[3];
	unsigned short	did[3];
} zro_ent;

#define MAX_FILE_OPEN_TRIES	20
#define WAIT_FOR_FILE_TIME	100	/* msec */

void zro_gettok(unsigned char **lp, unsigned char *top, unsigned *toktyp, mstr *tok);
void zro_search (mstr *objstr, zro_ent **objdir, mstr *srcstr, zro_ent **srcdir);

#endif
