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

#include "ladef.h"
#include "lmdef.h"

#include <ssdef.h>
#include <climsgdef.h>
#include <descrip.h>

/* la_store.c : License Administration function for creating new licenses
		from DCL command procedures
   used in    : license_adm.c
*/

int la_store(void)
{
	char		rp[32];			/* operator reply	*/
	char		*c_ptr;			/* db in main store	*/
	int4		status;
	uint4		bcs[3] = {0, 0, 0};
	int		v_arr[32];
	int		toupper();
	pak		*pak_ptr;			/* pak record		*/
	la_prolog	*prol;			/* db file prolog	*/

	error_def(LA_NOCNFDB);			/* No license created	*/
	error_def(LA_NEWCNF);			/* New license created	*/
	error_def(LA_BADENCR);

	if (NULL == (c_ptr = la_getdb(LADB)))	/* db in main storage	*/
		lib$signal(LA_NOCNFDB);
	prol = c_ptr;
	pak_ptr = (char *)c_ptr + prol->len;		/* place for new pak	*/
	la_initpak(prol->lastid, pak_ptr);		/* pak initialized	*/
	la_getcli(v_arr, pak_ptr);
	la_puthead(pak_ptr);
	la_putfldr(&(pak_ptr->pf));
	if (!la_encryt(pak_ptr->ph.n, &(pak_ptr->pd), (pak_ptr->ph.l[4] - pak_ptr->ph.l[2]), bcs))
		lib$signal(LA_BADENCR);
	else
		la_convert(pak_ptr->ph.cs, bcs);
	la_listpak(pak_ptr);
	(prol->N)++;				/* count of paks++	*/
	prol->len += pak_ptr->ph.l[0];		/* db file size++	*/
	prol->lastid = pak_ptr->pd.lid;		/* new last license ID	*/
	la_putdb(LADB, c_ptr);			/* db back to file	*/
	lm_putmsgu(LA_NEWCNF, 0, 0);
	la_freedb(c_ptr);
	return (SS$_NORMAL);
}
