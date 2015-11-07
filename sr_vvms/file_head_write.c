/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
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

error_def(ERR_DBFILOPERR);
error_def(ERR_DBNOTGDS);
error_def(ERR_DBOPNERR);

/*
 * This is a plain way to write file header.
 * User needs to take care of concurrency issue etc.
 * Parameters :
 *	fn : full name of a database file.
 *	header: Pointer to database file header structure (may not be in shared memory)
 *	len: length of header to write (should be either SGMNT_HDR_LEN or SIZEOF_FILE_HDR(header))
 */
boolean_t file_head_write(char *fn, sgmnt_data_ptr_t header, int4 len)
{
	int			header_size;
	int4			status1;
	uint4			status2;
	io_status_block_disk	iosb;
	struct FAB  		fab;
	struct XABFHC		xabfhc;

	header_size = SIZEOF_FILE_HDR(header);
	assert(SGMNT_HDR_LEN == len || header_size == len);
	fab = cc$rms_fab;
	fab.fab$l_fna = fn;
	fab.fab$b_fns = strlen(fn);
	fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
	fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_UPD ;
	fab.fab$l_fop = FAB$M_UFO;
	xabfhc = cc$rms_xabfhc;
	fab.fab$l_xab = &xabfhc;
	status1 = sys$open(&fab);
	if ((status1 & 1) == 0)
	{
 		gtm_putmsg(VARLSTCNT(6) ERR_DBOPNERR, 2, LEN_AND_STR(fn), status1, fab.fab$l_stv);
		return FALSE;
	}
	status2 = SS_NORMAL;
	DB_DO_FILE_WRITE(fab.fab$l_stv, 0, header, len, status1, status2);
	if (!(status1 & 1))
	{
		sys$dassgn(fab.fab$l_stv);	/* use sys$dassgn (not sys$close) since FAB$M_UFO was specified
						   in fab$l_fop in open */
		gtm_putmsg(VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), status1);
		return FALSE;
	}
	sys$dassgn(fab.fab$l_stv);
	return TRUE;
}
