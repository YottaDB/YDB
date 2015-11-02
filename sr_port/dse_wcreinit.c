/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "cli.h"
#include "dse.h"

#ifdef VMS
#define GET_CONFIRM(X,Y) {if(!cli_get_str("CONFIRMATION",(X),&(Y))) {rts_error(VARLSTCNT(1) ERR_DSEWCINITCON); \
	return;}}
#endif
#ifdef UNIX
#include "gtm_stdio.h"
#define GET_CONFIRM(X,Y) {PRINTF("CONFIRMATION: ");FGETS((X), (Y), stdin, fgets_res);Y = strlen(X);}

#endif

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short		crash_count;

void dse_wcreinit (void)
{
	unsigned char	*c;
	char		confirm[256];
	uint4		large_block;
	short		len;
	boolean_t	was_crit;
#	ifdef UNIX
	char		*fgets_res;
#	endif

	error_def(ERR_DSEWCINITCON);
	error_def(ERR_DSEINVLCLUSFN);
	error_def(ERR_DSEONLYBGMM);
        error_def(ERR_DBRDONLY);

        if (gv_cur_region->read_only)
                rts_error(VARLSTCNT(4) ERR_DBRDONLY, 2, DB_LEN_STR(gv_cur_region));

	if (cs_addrs->hdr->clustered)
	{
		rts_error(VARLSTCNT(1) ERR_DSEINVLCLUSFN);
		return;
	}
	if (cs_addrs->critical)
		crash_count = cs_addrs->critical->crashcnt;
	len = SIZEOF(confirm);
	GET_CONFIRM(confirm,len);
	if (confirm[0] != 'Y' && confirm[0] != 'y')
	{
		rts_error(VARLSTCNT(1) ERR_DSEWCINITCON);
		return;
	}
	if (cs_addrs->hdr->acc_meth != dba_bg && cs_addrs->hdr->acc_meth != dba_mm)
	{
		rts_error(VARLSTCNT(4) ERR_DSEONLYBGMM, 2, LEN_AND_LIT("WCINIT"));
		return;
	}
	was_crit = cs_addrs->now_crit;
	if (!was_crit)
		grab_crit(gv_cur_region);
	bt_init(cs_addrs);
	if (cs_addrs->hdr->acc_meth == dba_bg)
	{
		bt_refresh(cs_addrs);
		db_csh_ini(cs_addrs);
		db_csh_ref(cs_addrs);
	}
	if (!was_crit)
		rel_crit (gv_cur_region);

	return;
}
