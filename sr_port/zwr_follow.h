/****************************************************************
 *								*
<<<<<<< HEAD:sr_port/zwr_follow.h
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2004-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 451ab477 (GT.M V7.0-000):sr_port/trans_numeric.h
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef ZWR_FOLLOW_H_INCLUDED
#define ZWR_FOLLOW_H_INCLUDED

<<<<<<< HEAD:sr_port/zwr_follow.h
boolean_t zwr_follow(mval *u, mval *v);
=======
uint4 trans_numeric(mstr *log, boolean_t *is_defined, boolean_t ignore_errors);
gtm_uint8 trans_numeric_64(mstr *log, boolean_t *is_defined, boolean_t ignore_errors);
>>>>>>> 451ab477 (GT.M V7.0-000):sr_port/trans_numeric.h

#endif
