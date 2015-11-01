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

#define VAL_PRINT(VAL_TYPE, VAL_ADDR, VAL_LEN)\
{\
	val.mvtype = (VAL_TYPE);\
	val.str.addr = (VAL_ADDR);\
	val.str.len = (VAL_LEN);\
	op_write(&val);\
}
