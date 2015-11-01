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

#define MAXDEVPARLEN 1024
#define IOP_VAR_SIZE 255

#define IOP_OPEN_OK 1
#define IOP_USE_OK 2
#define IOP_CLOSE_OK 4

#define IOP_SRC_INT 1	/* source is integer */
#define IOP_SRC_STR 2	/* source is string */
#define IOP_SRC_MSK 3	/* source is character mask */
#define IOP_SRC_PRO 4	/* source is protection mask */
#define IOP_SRC_LNGMSK 5 /* source is int4 character mask */
#define IOP_SRC_TIME 6	/* source is the date-time string */

typedef struct
{
	unsigned char valid_with;
	unsigned char source_type;
} dev_ctl_struct;

#define IOP_DESC(a,b,c,d,e) b

enum io_params
{
#include "iop.h"
};

#undef IOP_DESC
