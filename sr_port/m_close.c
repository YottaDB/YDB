/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "io_params.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cmd.h"
#include "deviceparameters.h"

int m_close(void)
{
	static readonly unsigned char empty_plist[1] = { iop_eol };
	boolean_t	inddevparms;
	int		rval;
	oprtype		devpopr, plist, sopr;
	triple		*indref, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	inddevparms = FALSE;
	if (EXPR_FAIL == (rval = expr(&sopr, MUMPS_STR)))	/* NOTE assignment */
		return FALSE;
	if (TK_COLON != TREF(window_token))
	{	/* Single parameter */
		if (EXPR_INDR == rval)
		{	/* Indirect entire parameter list */
			make_commarg(&sopr, indir_close);
			return TRUE;
		} else	/* default device parms */
			plist = put_str((char *)empty_plist, SIZEOF(empty_plist));
	} else
	{	/* Have device parms. Determine type */
		advancewindow();
		if (TK_ATSIGN == TREF(window_token))
		{	/* Have indirect device parms */
			if (!indirection(&devpopr))
				return FALSE;
			indref = newtriple(OC_INDDEVPARMS);
			indref->operand[0] = devpopr;
			indref->operand[1] = put_ilit(IOP_CLOSE_OK);
			inddevparms = TRUE;
		} else
		{	/* Process device parameters now */
			if (!deviceparameters(&plist, IOP_CLOSE_OK))
				return FALSE;
		}
	}
	ref = newtriple(OC_CLOSE);
	ref->operand[0] = sopr;
	ref->operand[1] = !inddevparms ? plist : put_tref(indref);
	return TRUE;
}
