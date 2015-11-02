/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
	global = 0,
	local_name,
	local_sub,
	indir,
	last_obj
};

enum order_dir {
	forward = 0,
	backward,
	undecided,
	last_dir
};

STATICFNDCL boolean_t set_opcode(triple *r, oprtype *result, oprtype *result_ptr, oprtype *second_opr, enum order_obj object);

#endif
