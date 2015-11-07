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
#include <devdef.h>
#include <ssdef.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "is_file_identical.h"

bool is_gdid_file_identical(gd_id_ptr_t fid, char *filename, int4 filelen)
{
	uint4		status;
	struct FAB	fab;
	struct NAM	nam;
	char		es[MAX_FN_LEN + 1];
	error_def (ERR_FILEPARSE);

	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_nam = &(nam);
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = filename;
	fab.fab$b_fns = filelen;
	nam.nam$b_nop = NAM$M_NOCONCEAL;
	nam.nam$l_esa = es;
	nam.nam$b_ess = MAX_FN_LEN;
	if (RMS$_NORMAL == (status = sys$parse(&fab,0,0)))
		status = sys$search(&fab,0,0);
	if (RMS$_NORMAL != status)
	{
		rts_error(VARLSTCNT(6) ERR_FILEPARSE, 2, filelen, filename, status, fab.fab$l_stv);
		return FALSE;
	}

	return !(memcmp(&fid->dvi, &nam.nam$t_dvi, SIZEOF(fid->dvi)) || memcmp(&fid->fid, &nam.nam$w_fid, SIZEOF(fid->fid)));
}

/* is_file_identical()
 * 	returns TRUE	if the two files are identical,
 * 	returns FALSE	if either one of the files specified doesn't exist, or if they are different files.
 */

bool is_file_identical(char *filename1, char *filename2)
{
	uint4           status, filelen1, filelen2;
	struct FAB      fab1, fab2;
	struct NAM      nam1, nam2;
	char            es1[MAX_FN_LEN + 1], es2[MAX_FN_LEN + 1];
	error_def (ERR_FILEPARSE);

	fab1 = cc$rms_fab;
	nam1 = cc$rms_nam;
	fab1.fab$l_nam = &(nam1);
	fab1.fab$l_fop = FAB$M_NAM;
	fab1.fab$l_fna = filename1;
	fab1.fab$b_fns = filelen1 = strlen(filename1);
	nam1.nam$b_nop = NAM$M_NOCONCEAL;
	nam1.nam$l_esa = es1;
	nam1.nam$b_ess = MAX_FN_LEN;
	if (RMS$_NORMAL == (status = sys$parse(&fab1,0,0)))
		status = sys$search(&fab1,0,0);
	if (RMS$_NORMAL != status)
	{
		if (RMS$_FNF != status)	/* do not error out if one of these files do not exist */
			rts_error(VARLSTCNT(6) ERR_FILEPARSE, 2, filelen1, filename1, status, fab1.fab$l_stv);
		return FALSE;
	}
	fab2 = cc$rms_fab;
	nam2 = cc$rms_nam;
	fab2.fab$l_nam = &(nam2);
	fab2.fab$l_fop = FAB$M_NAM;
	fab2.fab$l_fna = filename2;
	fab2.fab$b_fns = filelen2 = strlen(filename2);
	nam2.nam$b_nop = NAM$M_NOCONCEAL;
	nam2.nam$l_esa = es2;
	nam2.nam$b_ess = MAX_FN_LEN;
	if (RMS$_NORMAL == (status = sys$parse(&fab2,0,0)))
		status = sys$search(&fab2,0,0);
	if (RMS$_NORMAL != status)
	{
		if (RMS$_FNF != status)	/* do not error out if one of these files do not exist */
			rts_error(VARLSTCNT(6) ERR_FILEPARSE, 2, filelen2, filename2, status, fab2.fab$l_stv);
		return FALSE;
	}

	return !(memcmp(&nam1.nam$t_dvi, &nam2.nam$t_dvi, SIZEOF(nam1.nam$t_dvi)) ||
		memcmp(&nam1.nam$w_fid, &nam2.nam$w_fid, SIZEOF(nam1.nam$w_fid)));
}

bool is_gdid_gdid_identical(gd_id_ptr_t fid_1, gd_id_ptr_t fid_2)
{	/* the file id (fid) is unique within a device (dvi), so the directory id (did) is redundant for uniqueness check */
	return !(memcmp(&fid_1->dvi, &fid_2->dvi, SIZEOF(fid_1->dvi)) || memcmp(&fid_1->fid, &fid_2->fid, SIZEOF(fid_1->fid)));
}

void set_gdid_from_file(gd_id_ptr_t fileid, char *filename, int4 filelen)
{
	uint4		status;
	struct FAB	fab;
	struct NAM	nam;
	char		es[MAX_FN_LEN + 1];
	error_def (ERR_FILEPARSE);

	fab = cc$rms_fab;
	nam = cc$rms_nam;
	fab.fab$l_nam = &(nam);
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = filename;
	fab.fab$b_fns = filelen;
	nam.nam$b_nop = NAM$M_NOCONCEAL;
	nam.nam$l_esa = es;
	nam.nam$b_ess = MAX_FN_LEN;
	if (RMS$_NORMAL == (status = sys$parse(&fab,0,0)))
		status = sys$search(&fab,0,0);
	if (RMS$_NORMAL == status)
	{
		memcpy(&fileid->dvi, &nam.nam$t_dvi, SIZEOF(fileid->dvi));
		memcpy(&fileid->did, &nam.nam$w_did, SIZEOF(fileid->did));
		memcpy(&fileid->fid, &nam.nam$w_fid, SIZEOF(fileid->fid));
	}
	else
		rts_error(VARLSTCNT(6) ERR_FILEPARSE, 2, filelen, filename, status, fab.fab$l_stv);
}
