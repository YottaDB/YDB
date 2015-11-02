/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _FNORDER_H_INC_
#define _FNORDER_H_INC_

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
