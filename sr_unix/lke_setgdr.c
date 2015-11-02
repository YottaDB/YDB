/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "io.h"
#include "gtmsecshr.h"
#include "cli.h"
#include "mutex.h"
#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "dpgbldir.h"
#include "gvcmy_rundown.h"
#include "mlkdef.h"
#include "lke.h"

GBLREF gd_region 	*gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_addr		*gd_header;
GBLREF mval		dollar_zgbldir;

error_def(ERR_NOTALLDBRNDWN);

void lke_setgdr(void)
{
	gd_region 	*r_top;
	mval		reset;
	bool		def;
	short		len;
	char		buf[256];
	int4		rundown_status = EXIT_NRM;			/* if gds_rundown went smoothly */
	static readonly char init_gdr[] = "gtmgbldir";

	gvcmy_rundown();
	gv_cur_region = gd_header->regions;
	r_top = gv_cur_region + gd_header->n_regions;
	for (gv_cur_region = gd_header->regions,
	  r_top = gv_cur_region + gd_header->n_regions;
	  gv_cur_region < r_top;
	  gv_cur_region++)
	{
		tp_change_reg();
		UNIX_ONLY(rundown_status |=) gds_rundown();
	}

	if (EXIT_NRM != rundown_status)
		rts_error(VARLSTCNT(1) ERR_NOTALLDBRNDWN);

	if (cli_present("gld"))
	{
		cli_get_value("gld", buf) ;
		def = FALSE;
		reset.mvtype = MV_STR;
		reset.str.len = STRLEN(buf);
		reset.str.addr = buf;
	}
	else
	{
		reset.mvtype = MV_STR;
		reset.str.len = SIZEOF(init_gdr) - 1;
		reset.str.addr = init_gdr;
	}

	zgbldir(&reset);
	cs_addrs = 0;
	cs_data = 0;
	region_init(FALSE) ;
#ifndef MUTEX_MSEM_WAKE
	mutex_sock_cleanup();
#endif
	gtmsecshr_sock_cleanup(CLIENT);
}

