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
#ifndef __LA_ENCRYPT_H__
#define __LA_ENCRYPT_H__

bool la_encrypt (
	short 		n  ,			/* encryption function number	*/
	char	 	*q , 			/* input sequence 		*/
	int 		len,			/* sequence length 		*/
	uint4 		bcs[]);			/* result, binary form 		*/

#endif
