/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "cmd.h"

#ifdef AUTORELINK_SUPPORTED
/* Routine to compile ZRUPDATE command
 *
 * Current syntax:
 *
 *   ZRUPDATE <object-file-path1>[,<object-file-path2>..
 *
 * Where:
 *
 *   object-file-path  - Is the path and filename of an object file that may include wildcards. We allow multiple specifications
 *			 separated by commas. We let cmd.c break these into separate commands.
 *
 * Parameters: none
 * Returncode: success (TRUE) and failure (FALSE)
 *
 * Future enhancement -  Allow paths to be grouped by surrounding with parens. At that point, this becomes a variable length
 *                       parameter list but for now tiz simplistic.
 */
int m_zrupdate(void)
{
	oprtype 	objfilespec;
	triple 		*triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (expr(&objfilespec, MUMPS_STR))
	{
		case EXPR_FAIL:
			return FALSE;
		case EXPR_GOOD:
			triptr = newtriple(OC_ZRUPDATE);
			triptr->operand[0] = put_ilit(1);		/* Currently single arg but that will change for phase2 */
			triptr->operand[1] = objfilespec;
			return TRUE;
		case EXPR_INDR:
			make_commarg(&objfilespec, indir_zrupdate);
			return TRUE;
	}
	assert(FALSE);
	return FALSE;
}
#endif /* USHBIN_SUPPORTED */
