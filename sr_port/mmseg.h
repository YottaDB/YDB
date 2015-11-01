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

/* mmseg is used by get_mmseg()/put_mmseg()/rel_mmseg()
 * get/put/rel_mmseg() manage virtual address space for MM segments.
 * We want to push MM segments to high address space so that there is enough
 * lower address space available for other uses like stacks etc. The current
 * scheme is for control segments to start from 1G, and MM segments to start
 * from 4G, and all the rest should stay below 1G.
 */
typedef struct mmseg_struct
{
	struct mmseg_struct	*next;
	sm_uc_ptr_t		begin;
	sm_uc_ptr_t		end;
} mmseg;

caddr_t get_mmseg(size_t size);
void put_mmseg(caddr_t begin, size_t size);
void rel_mmseg(caddr_t begin);


