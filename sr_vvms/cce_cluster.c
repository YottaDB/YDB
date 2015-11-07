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

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include <rmsdef.h>
#include <fab.h>
#include <xab.h>
#include <ssdef.h>
#include <iodef.h>
#include <descrip.h>
#include <climsgdef.h>
#include <efndef.h>
#include "cli.h"

#include "filestruct.h"
#include "util.h"
#include "timers.h"
#include "dbcx_ref.h"

void cce_cluster(void )
{
	enum db_ver		database;
	char			fn[256];
	struct FAB  		ccpfab;
	struct XABFHC		xab;
	sgmnt_data		*old_data, *dummy_data;
	short			iosb[4];
	unsigned short		fn_len;
	int4			status, size, cluster, space_needed;
	error_def(ERR_CCERDERR);
	error_def(ERR_CCEWRTERR);
	error_def(ERR_CCEDBCL);
	error_def(ERR_CCEDBNTCL);
	error_def(ERR_CCEBADFN);
	error_def(ERR_DBOPNERR);
	error_def(ERR_DBNOTGDS);
	error_def(ERR_BADDBVER);
	error_def(ERR_CCEBGONLY);
	$DESCRIPTOR(cluster_qualifier, "CLUSTER");

	fn_len = SIZEOF(fn);
	if (!cli_get_str("FILE",fn,&fn_len))
	{
		lib$signal(ERR_CCEBADFN);
		return;
	}
	ccpfab = cc$rms_fab;
	ccpfab.fab$l_fna = fn;
	ccpfab.fab$b_fns = fn_len;
	ccpfab.fab$b_fac = FAB$M_BIO | FAB$M_GET | FAB$M_PUT;
	ccpfab.fab$l_fop = FAB$M_UFO;
	xab = cc$rms_xabfhc;
	ccpfab.fab$l_xab = &xab;
	status = sys$open(&ccpfab);
	if (status != RMS$_NORMAL)
	{
		lib$signal(ERR_DBOPNERR, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna, status);
		return;
	}
	dummy_data = malloc(ROUND_UP(SIZEOF(sgmnt_data), OS_PAGELET_SIZE));
	status = sys$qiow(EFN$C_ENF, ccpfab.fab$l_stv, IO$_READVBLK, iosb, 0, 0, dummy_data,
			  ROUND_UP(SIZEOF(sgmnt_data), OS_PAGELET_SIZE), 1,0,0,0);
	if (status & 1)
		status = iosb[0];
	if ((status & 1) == 0)
	{
		lib$signal(ERR_CCERDERR, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna, status);
		sys$dassgn(ccpfab.fab$l_stv);
		free(dummy_data);
		return;
	}
/*
  Commented out as this is unused code and we are removing the old version database references
  (SE - 4/2005 V5.0)
  if (memcmp(&dummy_data->label[0], GDS_LABEL, GDS_LABEL_SZ))
  {	if (memcmp(&dummy_data->label[0], GDS_LABEL, GDS_LABEL_SZ - 3))
  {	lib$signal (ERR_DBNOTGDS, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna);
  status = sys$dassgn(ccpfab.fab$l_stv);
  assert(status & 1);
  free(dummy_data);
  return;
  }else
  {
  if (!memcmp(&dummy_data->label[GDS_LABEL_SZ - 3],GDS_V23,2))
  database = v23;
  else if (!memcmp(&dummy_data->label[GDS_LABEL_SZ - 3],GDS_V24,2))
  database = v24;
  else if (!memcmp(&dummy_data->label[GDS_LABEL_SZ - 3],GDS_V25,2))
  database = v25;
  else if (!memcmp(&dummy_data->label[GDS_LABEL_SZ - 3],GDS_V254,2))
  database = v254;
  else
  lib$signal (ERR_BADDBVER, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna);
  }
  }
  else
  database = v255;
*/

	if (dummy_data->acc_meth != dba_bg)
	{
		lib$signal(ERR_CCEBGONLY);
		status = sys$dassgn(ccpfab.fab$l_stv);
		assert(status & 1);
		free(dummy_data);
		return;
	}
	size = (LOCK_BLOCK(dummy_data) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(dummy_data);
	old_data = malloc(size);
	memcpy(old_data, dummy_data, SIZEOF(sgmnt_data));
	cluster = cli$present(&cluster_qualifier);
	if (cluster == CLI$_NEGATED)
		old_data->clustered = FALSE;
	else if (cluster == CLI$_PRESENT)
	{
		old_data->clustered = TRUE;
		old_data->trans_hist.lock_sequence = 0;
	}
	change_fhead_timer("STALE_INTERVAL", old_data->staleness, -50000000, TRUE);
	change_fhead_timer("RESPONSE_INTERVAL",old_data->ccp_response_interval, -600000000, TRUE);
	change_fhead_timer("QUANTUM_INTERVAL", old_data->ccp_quantum_interval, -10000000, FALSE);
	change_fhead_timer("TICK_INTERVAL", old_data->ccp_tick_interval, -1000000, FALSE);
	if (old_data->unbacked_cache)
	{
		space_needed = (old_data->n_bts + getprime(old_data->n_bts) + 1) * SIZEOF(bt_rec);
		if (space_needed > old_data->free_space)
		{
			old_data->n_bts = old_data->free_space/(2*SIZEOF(bt_rec));
			for (;;)
			{
				space_needed = (old_data->n_bts + getprime(old_data->n_bts) + 1) * SIZEOF(bt_rec);
				if (space_needed <= old_data->free_space)
				{
					old_data->bt_buckets = getprime(old_data->n_bts);
					break;
				}
				old_data->n_bts--;
			}
			util_out_open(0);
			util_out_print("Only have space for !UL cache records in clustered file !AD, adjusting file",TRUE,
				       old_data->n_bts, fn_len, fn);
		}
		old_data->free_space -= space_needed;
		old_data->unbacked_cache = FALSE;
	}
	status = dbcx_ref(old_data, ccpfab.fab$l_stv);
	if ((status & 1) == 0)
		lib$signal(ERR_CCEWRTERR, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna, status);
	if (cluster != CLI$_ABSENT)
		lib$signal ((old_data->clustered) ? ERR_CCEDBCL : ERR_CCEDBNTCL, 2, ccpfab.fab$b_fns, ccpfab.fab$l_fna);
	status = sys$dassgn(ccpfab.fab$l_stv);
	assert(status & 1);
	free(dummy_data);
	free(old_data);
	return;
}
