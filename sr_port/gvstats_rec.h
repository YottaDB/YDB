/****************************************************************
 *								*
 *	Copyright 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GVSTATS_REC_H_INCLUDED
#define GVSTATS_REC_H_INCLUDED

#define	TAB_GVSTATS_REC(A,B,C)	A,
enum gvstats_rec_type
{
#include "tab_gvstats_rec.h"
n_gvstats_rec_types
};
#undef TAB_GVSTATS_REC

typedef struct gvstats_rec_struct
{
#define	TAB_GVSTATS_REC(A,B,C)	gtm_uint64_t	A;
#include "tab_gvstats_rec.h"
} gvstats_rec_t;
#undef TAB_GVSTATS_REC

#endif
