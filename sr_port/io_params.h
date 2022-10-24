/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define MAX_COMPILETIME_DEVPARLEN	255	/* maximum length of device parameter literals allowed at compile time */
#define MAX_RUNTIME_DEVPARLEN		MAX_STRLEN	/* maximum length of device parameter allowed at run time */

#define	MAX_DISPLAYED_DEVPARLEN		1024	/* maximum length of device parameter displayed in errors messages */

/* The below macro values are chosen to be different from all values in the "data_element_size" column of "sr_port/iop.h"
 * (other than of course those rows where these macros show up in that column). All current values are SIZEOF() of a data type
 * that translates to a size of at most 32 bytes currently so defining these to be 255 and 254 ensures the uniqueness for now.
 */
#define IOP_VAR_SIZE_4BYTE	254
#define IOP_VAR_SIZE		255

#define	IOP_VAR_SIZE_LEN	1	/* 1-byte length field that precedes the actual string data */
#define	IOP_VAR_SIZE_4BYTE_LEN	4	/* 4-byte length field that precedes the actual string data */

#define	UPDATE_P_OFFSET(P_OFFSET, CH, PP)				\
{									\
	int	length;							\
									\
	switch(io_params_size[CH])					\
	{								\
	case IOP_VAR_SIZE:						\
		length = (unsigned char)*(PP->str.addr + P_OFFSET);	\
		length += IOP_VAR_SIZE_LEN;				\
		break;							\
	case IOP_VAR_SIZE_4BYTE:					\
		GET_LONG(length, PP->str.addr + P_OFFSET);		\
		length += IOP_VAR_SIZE_4BYTE_LEN;			\
		break;							\
	default:							\
		length = io_params_size[CH];				\
		break;							\
	}								\
	P_OFFSET += length;						\
}

#define IOP_OPEN_OK 1
#define IOP_USE_OK 2
#define IOP_CLOSE_OK 4

#define IOP_SRC_INT 1	/* source is integer */
#define IOP_SRC_STR 2	/* source is string */
#define IOP_SRC_MSK 3	/* source is character mask */
#define IOP_SRC_PRO 4	/* source is protection mask */
#define IOP_SRC_LNGMSK 5 /* source is int4 character mask */

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
