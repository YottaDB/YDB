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

#ifndef PROBECRIT_REC_H_INCLUDED
#define PROBECRIT_REC_H_INCLUDED
#define	TAB_PROBECRIT_REC(A,B,C)	A,
enum probecrit_rec_type
{
#	include "tab_probecrit_rec.h"
	n_probecrit_rec_types
};
#undef TAB_PROBECRIT_REC

typedef struct probecrit_rec_struct
{
#	define	TAB_PROBECRIT_REC(A,B,C)	gtm_uint64_t	A;
#	include "tab_probecrit_rec.h"
} probecrit_rec_t;
#undef TAB_PROBECRIT_REC
#endif
