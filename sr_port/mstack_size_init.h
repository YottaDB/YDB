/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MSTACK_SIZE_INIT_H_INCLUDED
#define MSTACK_SIZE_INIT_H_INCLUDED

#define MSTACK_MIN_SIZE 25
#define MSTACK_MAX_SIZE 10000
#define MSTACK_DEF_SIZE 272

void mstack_size_init(struct startup_vector *svec);

#endif /* MSTACK_SIZE_INIT_H_INCLUDED */
