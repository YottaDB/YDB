/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "filestruct.h"
#include "util.h"
#include "cli.h"
#include "dse.h"
#include "gtmmsg.h"
#include "gvcst_protos.h"

GBLREF block_id		patch_curr_blk;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short		crash_count;
GBLREF mval		dollar_zgbldir;
GBLREF gd_addr		*original_header;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key		*gv_currkey;

error_def(ERR_DSENOTOPEN);
error_def(ERR_NOGTCMDB);
error_def(ERR_NOREGION);
error_def(ERR_NOUSERDB);

void dse_f_reg(void)
{
	char		rn[MAX_RN_LEN];
	unsigned short	rnlen;
	int		i;
	boolean_t	found;
	gd_region	*regptr, *statsDBreg;

	rnlen = SIZEOF(rn);
	if (!cli_get_str("REGION", rn, &rnlen))
		return;
	if (('*' == rn[0]) && (1 == rnlen))
	{
		util_out_print("List of global directory:!_!AD!/", TRUE, dollar_zgbldir.str.len, dollar_zgbldir.str.addr);
		for (i = 0, regptr = original_header->regions; i < original_header->n_regions; i++, regptr++)
		{
			util_out_print("!/File  !_!AD", TRUE, regptr->dyn.addr->fname_len, &regptr->dyn.addr->fname[0]);
			util_out_print("Region!_!AD", TRUE, REG_LEN_STR(regptr));
		}
		return;
	}
	assert(rn[0]);
	for (i = 0; i < rnlen; i++)				/* Region names are always upper-case ASCII */
		rn[i] = TOUPPER(rn[i]);
	found = FALSE;
	for (i = 0, regptr = original_header->regions; i < original_header->n_regions ;i++, regptr++)
	{
		if (found = !memcmp(&regptr->rname[0], &rn[0], MAX_RN_LEN))
			break;
	}
	if (!found)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, rnlen, rn);
		return;
	}
	assert(!IS_STATSDB_REG(regptr));
	if (CLI_PRESENT == cli_present("STATS"))
	{	/* Go to corresponding STATSDB if present */
		if (!(RDBF_NOSTATS & regptr->reservedDBFlags))
		{
			BASEDBREG_TO_STATSDBREG(regptr, statsDBreg);
			assert(NULL != statsDBreg);
			if (!statsDBreg->open)
				gv_init_reg(statsDBreg, NULL);
			regptr = statsDBreg;
		} else
		{
			util_out_print("Error:  Region: !AD does not have an associated stats region", TRUE, REG_LEN_STR(regptr));
			return;
		}
	}
	if (regptr == gv_cur_region)
	{
		util_out_print("Error:  already in region: !AD", TRUE, REG_LEN_STR(gv_cur_region));
		return;
	}
	/* reg_cmcheck would have already been called for ALL regions at region_init time. In Unix, this would have set
	 * reg->dyn.addr->acc_meth to dba_cm if it is remote database. So we can safely use this to check if the region
	 * is dba_cm or not.
	 */
	if (dba_cm == regptr->dyn.addr->acc_meth)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_NOGTCMDB, 4, LEN_AND_LIT("DSE"), rnlen, rn);
		return;
	}
	if (!regptr->open)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DSENOTOPEN, 2, rnlen, rn);
		return;
	}
	if (cs_addrs->now_crit)
		util_out_print("Warning:  now leaving region in critical section: !AD", TRUE, REG_LEN_STR(gv_cur_region));
	gv_cur_region = regptr;
	gv_target = NULL;	/* to prevent out-of-sync situations between gv_target and cs_addrs */
	assert(IS_REG_BG_OR_MM(gv_cur_region));
	cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
	cs_data = cs_addrs->hdr;
	if (cs_addrs && cs_addrs->critical)
		crash_count = cs_addrs->critical->crashcnt;
	util_out_print("!/File  !_!AD", TRUE, DB_LEN_STR(gv_cur_region));
	util_out_print("Region!_!AD!/", TRUE, REG_LEN_STR(gv_cur_region));
	patch_curr_blk = get_dir_root();
	gv_init_reg(gv_cur_region, NULL);
	return;
}
