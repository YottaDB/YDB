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

#ifndef SGNL_INCLUDED
#define SGNL_INCLUDED

void sgnl_gvnulsubsc(void);
void sgnl_gvreplerr(void);
void sgnl_gvundef(void);
int sgnl_assert(unsigned int filesz, unsigned char *file, unsigned int linenum);


#endif /* SGNL_INCLUDED */
