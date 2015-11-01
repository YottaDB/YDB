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

#ifndef FNPC_INCLUDED
#define FNPC_INCLUDED

#define FNPC_STRLEN_MIN 15
#define FNPC_MAX 50
#define FNPC_ELEM_MAX 80

typedef struct fnpc_struct
{
	mstr		last_str;			/* The last string (addr/len) we used in cache */
	int		delim;				/* delimiter used in $piece */
	int		npcs;				/* Number of pieces for which values are filled in */
	int		indx;				/* The index of this piece */
	unsigned int	*pcoffmax;			/* Address of last element in pstart array */
	unsigned int	pstart[FNPC_ELEM_MAX + 1];	/* Where each piece starts (last elem holds end of last piece) */
} fnpc;

typedef struct
{
	fnpc		*fnpcsteal;			/* Last stolen cache element */
	fnpc		*fnpcmax;			/* (use addrs to avoid array indexing) */
	fnpc		fnpcs[FNPC_MAX];
} fnpc_area;

#endif
