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
#include <errno.h>
#include <rms.h>
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include <iodef.h>
#include "gtmmsg.h"
#include "gtm_file_stat.h"


/* Gets expanded path of a file.
 * NOTE: Since gtm_file_stat now expands a file name with full path, consider nixing this routine : Layek 5/10/2
 * Output Parameter
 *	uint4 *status : error number
 * Returns:
 *	TRUE: if file path is expanded
 *	FALSE: if file path cannot be expanded
 */
boolean_t get_full_path(char *name, unsigned int len, char *exp_file_name,
			unsigned int *exp_name_len, int max_len, uint4 *status)
{
	struct FAB	fab;
	struct NAM	nam;
	char		es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN];
	char		*src, *srctop, *dest, *destmax;
	error_def(ERR_FILENAMETOOLONG);

	nam = cc$rms_nam;
	/* Note From Documentation :
	 * 	The nam$l_esa and nam$b_ess fields must be specified (nonzero) for wildcard character processing.
	 *      nam$l_rsa required for wildcard character processing */
	nam.nam$l_rsa = name_buffer; 		/* Resultant string area address: specifies name, type,
						 * 	and version of last file found */
	nam.nam$b_rss = SIZEOF(name_buffer);	/* Resultant String Area size. */
	nam.nam$l_esa = es_buffer;		/* Expanded String area address: specifies file name, type,
						 * 	and version of file. */
	nam.nam$b_ess = SIZEOF(es_buffer);	/* Size of Expanded String area */
	nam.nam$b_nop = NAM$M_NOCONCEAL;	/* indicates that when a concealed device logical name is present,
						 * 	the concealed device logical name is to be replaced by the
						 * 	actual physical device name in the expanded string. */
	fab = cc$rms_fab;
	fab.fab$l_nam = &nam;
	fab.fab$l_fop = FAB$M_NAM;
	fab.fab$l_fna = name;			/* File specification string address. */
	fab.fab$b_fns = len;			/* File specification string size. */
	if ((*status = sys$parse(&fab, 0, 0)) != RMS$_NORMAL)
		return FALSE;
	*status = sys$search(&fab, 0, 0);
	if (*status != RMS$_NORMAL && *status != RMS$_NMF && *status != RMS$_FNF)
		return FALSE;
	/* For returned length eliminate version info */
	fncpy_nover(nam.nam$l_rsa, nam.nam$b_rsl, exp_file_name, *exp_name_len);
	return TRUE;
}
