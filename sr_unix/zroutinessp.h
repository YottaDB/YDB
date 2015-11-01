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

#ifndef __ZROUTINESSP_H__
#define __ZROUTINESSP_H__

typedef	struct zro_ent_type
{
	unsigned char	type;
	unsigned	count;
	mstr		str;
} zro_ent;

#define ZRO_MAX_ENTS		4096
#define ZRO_TYPE_COUNT		1
#define ZRO_TYPE_OBJECT		2
#define ZRO_TYPE_SOURCE		3
#define ZRO_TYPE_OBJLIB		4
#define ZROUTINE_LOG		"$gtmroutines"

#define ZRO_EOL 0
#define ZRO_IDN 1
#define ZRO_DEL ' '
#define ZRO_LBR '('
#define ZRO_RBR ')'

int zro_gettok(char **lp, char *top, mstr *tok);
void zsrch_clr(int indx);

#endif
