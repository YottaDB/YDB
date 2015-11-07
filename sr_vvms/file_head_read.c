/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
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
#include <rms.h>
#include <climsgdef.h>
#include <iodef.h>
#include <efndef.h>
#include <ssdef.h>
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmio.h"
#include "gtmmsg.h"
#include "iosb_disk.h"
#include "iosp.h"
/*
 * This is a plain way to read file header.
 * User needs to take care of concurrency issue etc.
 * Parameters :
 *	fn : full name of a database file.
 *	header: Pointer to database file header structure (may not be in shared memory)
 *	len: size of header (may be just SGMNT_HDR_LEN or SIZEOF_FILE_HDR_MAX)
 */
boolean_t file_head_read(char *fn, sgmnt_data_ptr_t header, int4 len)
{
	int			header_size;
	int4			status1;
	uint4			status2;
	io_status_block_disk	iosb;
	struct FAB  		fab;
	struct XABFHC		xabfhc;

	error_def(ERR_DBOPNERR);
	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBNOTGDS);

	header_size = SIZEOF(sgmnt_data);
	fab = cc$rms_fab;
	fab.fab$l_fna = fn;
	fab.fab$b_fns = strlen(fn);
	fab.fab$b_fac = FAB$M_GET;
	fab.fab$l_fop = FAB$M_UFO;
	fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
	xabfhc = cc$rms_xabfhc;
	fab.fab$l_xab = &xabfhc;
	status1 = sys$open(&fab);
	if ((status1 & 1) == 0)
	{
 		gtm_putmsg(VARLSTCNT(6) ERR_DBOPNERR, 2, LEN_AND_STR(fn), status1, fab.fab$l_stv);
		return FALSE;
	}
	status2 = SS_NORMAL;
	DO_FILE_READ(fab.fab$l_stv, 0, header, header_size, status1, status2);
	if (!(status1 & 1))
	{
		sys$dassgn(fab.fab$l_stv);
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), status1);
		return FALSE;
	}
	if (memcmp(header->label, GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		sys$dassgn(fab.fab$l_stv);
		gtm_putmsg(VARLSTCNT(4) ERR_DBNOTGDS, 2, LEN_AND_STR(fn));
		return FALSE;
	}
	assert(MASTER_MAP_SIZE_MAX >= MASTER_MAP_SIZE(header));
	assert(SGMNT_HDR_LEN == len || SIZEOF_FILE_HDR(header) <= len);
	if (SIZEOF_FILE_HDR(header) <= len)
	{
		status2 = SS_NORMAL;
		DO_FILE_READ(fab.fab$l_stv, ROUND_UP(SGMNT_HDR_LEN + 1, DISK_BLOCK_SIZE), MM_ADDR(header),
				MASTER_MAP_SIZE(header), status1, status2);
		if (!(status1 & 1))
		{
			sys$dassgn(fab.fab$l_stv);
			gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), status1);
			return FALSE;
		}
	}
	sys$dassgn(fab.fab$l_stv);	/* use sys$dassgn (not sys$close) since FAB$M_UFO was specified in fab$l_fop in open */
	return TRUE;
}
