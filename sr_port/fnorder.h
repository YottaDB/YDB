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
