/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/*
 * This function creates protected memory. The created protected memory is always multiple of OS_PAGE_SIZE, so it is advisable to
 * have input 'size' in the multiple of OS_PAGE_SIZE. The return value is the starting address of the protected memory.
 */
void memprot(mstr *base, uint4 size);
