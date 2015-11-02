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

#ifndef GTMIMAGENAME_DEF
#define GTMIMAGENAME_DEF

typedef struct
{
	char	*imageName;
	int	imageNameLen;
} gtmImageName;

enum gtmImageTypes
{
#define IMAGE_TABLE_ENTRY(A,B)	A,
#include "gtmimagetable.h"
#undef IMAGE_TABLE_ENTRY
	n_image_types
};

#define GTMIMAGENAMETXT(x) gtmImageNames[x].imageNameLen, gtmImageNames[x].imageName
#endif
