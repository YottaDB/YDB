/****************************************************************
 *								*
 *	Copyright 2008, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gvstats_rec.h"

void	gvstats_rec_csd2cnl(sgmnt_addrs *csa)
{
	memcpy(&csa->nl->gvstats_rec, &csa->hdr->gvstats_rec, SIZEOF(gvstats_rec_t));
}

void	gvstats_rec_cnl2csd(sgmnt_addrs *csa)
{
	memcpy(&csa->hdr->gvstats_rec, &csa->nl->gvstats_rec, SIZEOF(gvstats_rec_t));
}

void	gvstats_rec_upgrade(sgmnt_addrs *csa)
{
	node_local_ptr_t	cnl;
	sgmnt_data_ptr_t	csd;
	int			index;

	csd = csa->hdr;
	cnl = csa->nl;
	/* csd still contains gvstats info in old place. Copy over to new location */
	cnl->gvstats_rec.n_nontp_retries_0 = csd->filler_n_retries[0];
	cnl->gvstats_rec.n_nontp_retries_1 = csd->filler_n_retries[1];
	cnl->gvstats_rec.n_nontp_retries_2 = csd->filler_n_retries[2];
	cnl->gvstats_rec.n_nontp_retries_3 = csd->filler_n_retries[3];
	cnl->gvstats_rec.n_set             = csd->filler_n_puts;
	cnl->gvstats_rec.n_kill            = csd->filler_n_kills;
	cnl->gvstats_rec.n_query           = csd->filler_n_queries;
	cnl->gvstats_rec.n_get             = csd->filler_n_gets;
	cnl->gvstats_rec.n_order           = csd->filler_n_order;
	cnl->gvstats_rec.n_zprev           = csd->filler_n_zprevs;
	cnl->gvstats_rec.n_data            = csd->filler_n_data;
	/* No longer maintained            : csd->filler_n_puts_duplicate       */
	cnl->gvstats_rec.n_tp_readwrite    = csd->filler_n_tp_updates;
	/* No longer maintained            : csd->filler_n_tp_updates_duplicate */
	cnl->gvstats_rec.n_tp_tot_retries_0 = csd->filler_n_tp_retries[0];
	cnl->gvstats_rec.n_tp_tot_retries_1 = csd->filler_n_tp_retries[1];
	cnl->gvstats_rec.n_tp_tot_retries_2 = csd->filler_n_tp_retries[2];
	cnl->gvstats_rec.n_tp_tot_retries_3 = csd->filler_n_tp_retries[3];
	cnl->gvstats_rec.n_tp_tot_retries_4 = csd->filler_n_tp_retries[4];
	for (index = 5; index < 12; index++)
		cnl->gvstats_rec.n_tp_tot_retries_4 += csd->filler_n_tp_retries[index];
	cnl->gvstats_rec.n_tp_cnflct_retries_0 = csd->filler_n_tp_retries_conflicts[0];
	cnl->gvstats_rec.n_tp_cnflct_retries_1 = csd->filler_n_tp_retries_conflicts[1];
	cnl->gvstats_rec.n_tp_cnflct_retries_2 = csd->filler_n_tp_retries_conflicts[2];
	cnl->gvstats_rec.n_tp_cnflct_retries_3 = csd->filler_n_tp_retries_conflicts[3];
	cnl->gvstats_rec.n_tp_cnflct_retries_4 = csd->filler_n_tp_retries_conflicts[4];
	for (index = 5; index < 12; index++)
		cnl->gvstats_rec.n_tp_cnflct_retries_4 += csd->filler_n_tp_retries_conflicts[index];
	/* Nullify statistics that were formerly in use but no longer so */
	csd->unused_dsk_reads.curr_count = 0;
	csd->unused_dsk_reads.cumul_count = 0;
	csd->unused_dsk_writes.curr_count = 0;
	csd->unused_dsk_writes.cumul_count = 0;
}
