/****************************************************************
 *								*
 * Copyright (c) 2008-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GVSTATS_REC_H_INCLUDED
#define GVSTATS_REC_H_INCLUDED
#include "gtm_atomic.h"

/* Note gvstats_rec exists in both sgmnt_data (file header) and in node_local. The reason
 * for this is so that gvstats can be updated by read-only processes which would not be
 * able to update the read-only file header. The gvstats in node_local are the ones that
 * get updated and are peridically copied back to the fileheader and during fileheader
 * flushes to keep them up to date.
 */

#define	TAB_GVSTATS_REC(A,B,C)	A,
enum gvstats_rec_type
{
#include "tab_gvstats_rec.h"
n_gvstats_rec_types
};
#undef TAB_GVSTATS_REC

typedef struct gvstats_rec_struct
{
#define	TAB_GVSTATS_REC(A,B,C)	gtm_atomic_uint_least64_t	A;
#include "tab_gvstats_rec.h"
} gvstats_rec_t;
#undef TAB_GVSTATS_REC

typedef struct gvstats_rec_csd_struct
{
#define	TAB_GVSTATS_REC(A,B,C)	gtm_atomic_uint_least64_t	A;
#include "tab_gvstats_rec.h"
} gvstats_rec_csd_t;
#undef TAB_GVSTATS_REC

#endif
