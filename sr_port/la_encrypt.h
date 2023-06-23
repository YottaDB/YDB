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
#ifndef LA_ENCRYPT_H_INCLUDED
#define LA_ENCRYPT_H_INCLUDED

bool la_encrypt (
	short 		n  ,			/* encryption function number	*/
	char	 	*q , 			/* input sequence 		*/
	int 		len,			/* sequence length 		*/
	uint4 		bcs[]);			/* result, binary form 		*/

#endif
