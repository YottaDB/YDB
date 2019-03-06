/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef FNORDER_H_INCLUDED
#define FNORDER_H_INCLUDED

enum order_obj {
	GLOBAL = 0,
	LOCAL,
	LOCAL_NAME,
	INDIRECT,
	LAST_OBJECT
};

enum order_dir {
	FORWARD = 0,
	BACKWARD,
	TBD,
	LAST_DIRECTION
};

STATICFNDCL boolean_t set_opcode(triple *r, oprtype *result, oprtype *result_ptr, oprtype *second_opr, enum order_obj object);

#endif
