/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 7a1d2b3e... GT.M V6.3-007
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

#endif
