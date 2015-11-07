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
#include "util.h"
#include "gtmmsg.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_file_stat.h"

GBLREF bool incremental;
GBLREF bool in_backup;

/* Checks the status of a file.
 * Output Parameter
 *	uint4 *status : error number
 * Returns:
 *	FILE_NOT_FOUND: if file is not presnt
 *	FILE_PRESENT:	if file is present
 *	FILE_READONLY|FILE_PRESENT: if file is readonly
 *	FILE_STAT_ERROR: if error happens during this module.
 *	Side Effect:    Except for FILE_STAT_ERROR passed "ret" will have expanded file name with path.
 *	Note: This routine now removes version info in "ret". In case caller need that we need to change this module.
 */
int gtm_file_stat(mstr *file, mstr *def, mstr *ret, boolean_t check_prv, uint4 *status)
{
	char		*ptr;
	unsigned char	es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN];
	int		retstat;
	struct FAB 	fab;
	struct NAM 	nam;

	nam = cc$rms_nam;
	/* Note From Documentation :
	 * 	The nam$l_esa and nam$b_ess fields must be specified (nonzero) for wildcard character processing.
	 *      nam$l_rsa required for wildcard character processing */
	nam.nam$l_rsa = name_buffer; 		/* Resultant string area address: specifies name, type,
						 * 	and version of last file found */
	nam.nam$b_rss = SIZEOF(name_buffer);	/* Buffer size for l_rsa: For sys$search */
	nam.nam$l_esa = es_buffer;		/* Expanded String area address: specifies file name, type,
						 *	and version of file */
	nam.nam$b_ess = SIZEOF(es_buffer);	/* Size of Expanded String area */
	nam.nam$b_nop = NAM$M_NOCONCEAL;	/* Indicates that when a concealed device logical name is present,
						 * 	the concealed device logical name is to be replaced by the
						 * 	actual physical device name in the expanded string. */
	fab = cc$rms_fab;
	fab.fab$l_nam = &nam;
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = file->addr;		/* File specification string address. */
	fab.fab$b_fns = file->len;		/* File specification string size. */
	if (NULL != def)
	{
		fab.fab$l_dna = def->addr;	/* Default file specification string. */
		fab.fab$b_dns = def->len;	/* Default file specification string size. */
	}
	/* sys$parse is done in order to initialize the NAM or NAML block appropriately */
	if ((*status = sys$parse(&fab, 0, 0)) != RMS$_NORMAL)
		return FILE_STAT_ERROR;
	if (in_backup && !incremental && (fab.fab$l_dev & DEV$M_SQD))
	{
		util_out_print("MUPIP cannot backup to a magnetic tape",TRUE);
		return FILE_NOT_FOUND;
	}
	*status = sys$search(&fab, 0, 0);
	switch(*status)
	{
	case RMS$_NORMAL:
		retstat = FILE_PRESENT;
		break;
	case RMS$_NMF:
	case RMS$_FNF:
		retstat = FILE_NOT_FOUND;
		break;
	case RMS$_PRV:
		if (check_prv)
		{
			retstat = (FILE_PRESENT | FILE_READONLY);
			break;
		}
	default:
		return FILE_STAT_ERROR;
	}
	if (NULL != ret)
		/* For returned length eliminate version info */
		fncpy_nover(nam.nam$l_rsa, nam.nam$b_rsl, ret->addr, ret->len);
	if (!check_prv || FILE_PRESENT != retstat)
		return retstat;
	fab.fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
	fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_UPD ;
	fab.fab$l_fop = FAB$M_UFO;
	*status = sys$open(&fab);
	if (RMS$_PRV == *status)
		retstat = retstat | FILE_READONLY;
	if ((*status) & 1) /* if successful open */
		sys$dassgn(fab.fab$l_stv);
	else if (RMS$_PRV != *status)
		return FILE_STAT_ERROR;
	return retstat;
}
