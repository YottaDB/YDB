/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MLK_SHRBLK_CREATE_INCLUDED
#define MLK_SHRBLK_CREATE_INCLUDED

mlk_shrblk_ptr_t mlk_shrblk_create(mlk_pvtblk *p, unsigned char *val, int len, 	 mlk_shrblk_ptr_t par, ptroff_t *ptr, int nshrs);

#endif /* MLK_SHRBLK_CREATE_INCLUDED */
