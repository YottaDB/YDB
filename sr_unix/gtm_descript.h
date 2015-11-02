/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_DESCRIPT_INCLUDED
#define GTM_DESCRIPT_INCLUDED
#include "gtm_sizeof.h"

int mumps_call();


typedef struct  {
	short	len;
	short 	type;
	void	*val;
} gtm_descriptor;

typedef struct  {
	unsigned int	len;
	unsigned int	type;
	void		*val;
} gtm32_descriptor;

/* legal types */

#define	DSC_K_DTYPE_T	1
#define	GTM_ARRAY_OF_CHARS	DSC_K_DTYPE_T

#define DSC_K_DTYPE_D	2
#define	GTM_DOUBLE	DSC_K_DTYPE_D

#define	DSC_K_DTYPE_B	3
#define	GTM_CHAR	DSC_K_DTYPE_B

#define	DSC_K_DTYPE_BU	4
#define	GTM_UNSIGNED_CHAR	DSC_K_DTYPE_BU

#define	DSC_K_DTYPE_W	5
#define GTM_SHORT	DSC_K_DTYPE_W

#define	DSC_K_DTYPE_WU	6
#define GTM_UNSIGNED_SHORT	DSC_K_DTYPE_WU

#define	DSC_K_DTYPE_L	7
#define GTM_LONG	DSC_K_DTYPE_L

#define	DSC_K_DTYPE_LU	8
#define GTM_UNSIGNED_LONG	DSC_K_DTYPE_LU

#define	DSC_K_DTYPE_F	9
#define GTM_FLOAT	DSC_K_DTYPE_F

#define GTM_MODE	10
#define GTM_DELIMITER	10

#define DESCRIPTOR(x,y)	{x.type = GTM_ARRAY_OF_CHARS; x.len = SIZEOF(y) - 1; x.val = y;}
#define DESC_MODE(x,y) {x.type=GTM_MODE; x.len=SIZEOF(y); x.val=(void*)&y;}
#define DESC_CHAR(x,y) {x.type=GTM_ARRAY_OF_CHARS; x.len=SIZEOF(y)-1; x.val=y;}
#define DESC_ZERO(x) {x.type=0; x.len=0;}
#define DESC_LONG(x,y) {x.type=GTM_LONG; x.len=SIZEOF(y); x.val=&y;}
#define DESC_DELIM(x,y) {x.type=GTM_DELIMITER;x.len=SIZEOF(y);x.val=(void *)&y;}

#endif
