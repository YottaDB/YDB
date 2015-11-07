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
#include <rms.h>
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"
#include <iodef.h>
#include "probe.h"

GBLREF	ccp_db_header	*ccp_reg_root;

static	const	char	header_label[] = "<ccp_db_header @ -------------->",
#define header_offset	17	/*        -----------------^		*/
			region_label[] = "<gd_region @ ------------------>",
#define region_offset	13	/*        -------------^		*/
			addrs_label[]  = "<sgmnt_addrs @ ---------------->",
#define addrs_offset	15	/*        ---------------^		*/
			node_label[]   = "<node_local @ ----------------->",
#define node_offset	14	/*        --------------^		*/
			jnl_label[]    = "<jnl_private_control @ -------->",
#define jnl_offset	23	/*        -----------------------^	*/
			data_label1[]  = "<sgmnt_data @ ",
#define data_offset	14	/*        --------------^	*/
			data_label2[]  = " starts in next block>",
			end_label[]    = "<End of data>",
			filename[]     = "SYS$MANAGER:CCPDUMP.DMP";

#define READ		FALSE
#define RECORD_SIZE	1024


void ccp_dump(void)
{
	ccp_db_header	*db;
	sgmnt_data	dummy;
	struct FAB	fab;
	struct RAB	rab;
	char		*c, buffer[RECORD_SIZE];
	int		i;


	if (ccp_reg_root == NULL)
		return;
	fab = cc$rms_fab;
	fab.fab$b_fns = SIZEOF(filename - 1);
	fab.fab$l_fna = filename;
	fab.fab$w_mrs = RECORD_SIZE;
	fab.fab$b_rfm = FAB$C_FIX;
	fab.fab$b_fac = FAB$M_PUT;
	fab.fab$l_fop = FAB$M_CBT | FAB$M_MXV | FAB$M_TEF;
	dummy.n_bts = WC_MAX_BUFFS;
	dummy.bt_buckets = getprime(WC_MAX_BUFFS);
	fab.fab$w_deq = fab.fab$l_alq
		      = LOCK_BLOCK(&dummy);
	switch (sys$create(&fab))
	{
	case RMS$_NORMAL:
	case RMS$_CREATED:
	case RMS$_SUPERSEDE:
	case RMS$_FILEPURGED:
		break;
	default:
		return;
	}
	rab = cc$rms_rab;
	rab.rab$w_rsz = RECORD_SIZE;
	rab.rab$l_fab = &fab;
	if (sys$connect(&rab) != RMS$_NORMAL)
		return;
	for (db = ccp_reg_root;  db != NULL;  db = db->next)
	{
		memset(buffer, 0, SIZEOF(buffer));
		c = buffer;
		if (probe(SIZEOF(ccp_db_header), db, READ))
		{
			memcpy(c, header_label, SIZEOF(header_label) - 1);
			(void)i2hex_nofill(db, c + header_offset, SIZEOF(c) * 2);
			c += SIZEOF(header_label) - 1;
			/* ccp_db_header is defined in CCP.H */
			memcpy(c, db, SIZEOF(ccp_db_header));
			c += ROUND_UP(SIZEOF(ccp_db_header), 32);
		}
		if (probe(SIZEOF(gd_region), db->greg, READ))
		{
			memcpy(c, region_label, SIZEOF(region_label) - 1);
			(void)i2hex_nofill(db->greg, c + region_offset, SIZEOF(c) * 2);
			c += SIZEOF(region_label) - 1;
			/* gd_region is defined in GDSFHEAD.H */
			memcpy(c, db->greg, SIZEOF(gd_region));
			c += ROUND_UP(SIZEOF(gd_region), 32);
		}
		if (probe(SIZEOF(sgmnt_addrs), db->segment, READ))
		{
			memcpy(c, addrs_label, SIZEOF(addrs_label) - 1);
			(void)i2hex_nofill(db->segment, c + addrs_offset, SIZEOF(c) * 2);
			c += SIZEOF(addrs_label) - 1;
			/* sgmnt_addrs is defined in GDSFHEAD.H */
			memcpy(c, db->segment, SIZEOF(sgmnt_addrs));
			c += ROUND_UP(SIZEOF(sgmnt_addrs), 32);
			if (probe(SIZEOF(node_local), db->segment->nl, READ))
			{
				memcpy(c, node_label, SIZEOF(node_label) - 1);
				(void)i2hex_nofill(db->segment->nl, c + node_offset, SIZEOF(c) * 2);
				c += SIZEOF(node_label) - 1;
				/* node_local is defined in GDSBT.H */
				memcpy(c, db->segment->nl, SIZEOF(node_local));
				c += ROUND_UP(SIZEOF(node_local), 32);
			}
			if (db->segment->jnl != NULL  &&  probe(SIZEOF(jnl_private_control), db->segment->jnl, READ))
			{
				memcpy(c, jnl_label, SIZEOF(jnl_label) - 1);
				(void)i2hex_nofill(db->segment->jnl, c + jnl_offset, SIZEOF(c) * 2);
				c += SIZEOF(jnl_label) - 1;
				/* jnl_private_control is defined in JNL.H */
				memcpy(c, db->segment->jnl, SIZEOF(jnl_private_control));
				c += ROUND_UP(SIZEOF(jnl_private_control), 32);
			}
		}
		memcpy(c, data_label1, SIZEOF(data_label1) - 1);
		i = i2hex_nofill(db->glob_sec, c + data_offset, SIZEOF(c) * 2);
		memcpy(c + SIZEOF(data_label1) - 1 + i, data_label2, SIZEOF(data_label2) - 1);
		rab.rab$l_rbf = buffer;
		if (sys$put(&rab) != RMS$_NORMAL)
		{
			sys$close(&fab);
			return;
		}
		/* db->glob_sec points to sgmnt_data, defined in GDSFHEAD.H */
		if (probe(RECORD_SIZE, db->glob_sec, READ))
		{
			i = DIVIDE_ROUND_UP((LOCK_BLOCK(db->glob_sec) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(db->glob_sec)
				+ CACHE_CONTROL_SIZE(db->glob_sec), OS_PAGELET_SIZE);
			for (c = db->glob_sec;  i > 0;  c += RECORD_SIZE, i -= (RECORD_SIZE / OS_PAGELET_SIZE))
				if (probe(RECORD_SIZE, c, READ))
				{
					rab.rab$l_rbf = c;
					if (sys$put(&rab) != RMS$_NORMAL)
					{
						sys$close(&fab);
						return;
					}
				} else
					break;
		}
	}
	memset(buffer, 0, RECORD_SIZE);
	memcpy(buffer, end_label, SIZEOF(end_label) - 1);
	rab.rab$l_rbf = buffer;
	sys$put(&rab);
	sys$close(&fab);
	return;
}
