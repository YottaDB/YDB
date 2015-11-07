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

#include "mdef.h"

#include "gtm_string.h"

#include <descrip.h>
#include <climsgdef.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gds_rundown.h"
#include "dpgbldir.h"
#include "gvcmy_rundown.h"
#include "mlkdef.h"
#include "lke.h"
#include "tp_change_reg.h"

GBLREF gd_region 	*gv_cur_region;
GBLREF sgmnt_data	*cs_data;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_addr		*gd_header;

void lke_setgdr(void)
{
	int4		cli$present(), cli$get_value();
	gd_region 	*r_top;
	mval		reset;
	bool		def;
	short		len;
	char		buf[256];
	$DESCRIPTOR	(dbuf,buf);
	static readonly $DESCRIPTOR(dent,"GLD");
	static readonly unsigned char init_gdr[] = "GTM$GBLDIR";

	gvcmy_rundown();
	for (gv_cur_region = gd_header->regions, r_top = gv_cur_region + gd_header->n_regions; gv_cur_region < r_top;
			gv_cur_region++)
	{
		tp_change_reg();
		gds_rundown();
	}
	if (cli$present(&dent)==CLI$_PRESENT)
	{
		cli$get_value(&dent,&dbuf,&len);
		def = FALSE;
		reset.mvtype = MV_STR;
		reset.str.len = len;
		reset.str.addr = &buf;
	}
	else
	{
		reset.mvtype = MV_STR;
		reset.str.len = sizeof (init_gdr) - 1;
		reset.str.addr = &init_gdr;
	}
	zgbldir(&reset);
	cs_addrs = 0;
	cs_data = 0;
	region_init(TRUE) ;
}
