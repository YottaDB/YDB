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
#include "indir_enum.h"
#include "toktyp.h"
#include "iotimer.h"
#include "io_params.h"
#include "advancewindow.h"
#include "cmd.h"
#include "deviceparameters.h"
#include "mdq.h"

LITREF mval	literal_null;

int m_open(void)
{
	boolean_t	is_timeout, inddevparms;
	static readonly unsigned char empty_plist[1] = { iop_eol };
	int		rval;
	opctype		opcd;
	oprtype		devpopr, mspace, plist, sopr, timeout;
	triple		*indref, *ref1, *ref2, tmpchain;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	inddevparms = FALSE;
	if (EXPR_FAIL == (rval = expr(&sopr, MUMPS_STR)))	/* NOTE assignment */
		return FALSE;
	if (TK_COLON != TREF(window_token))
	{	/* Single arg specified */
		if (EXPR_INDR == rval)
		{	/* All arguments indirect */
			make_commarg(&sopr, indir_open);
			return TRUE;
		} else	/* Only device given, default device parms */
			plist = put_str((char *)empty_plist, SIZEOF(empty_plist));
	} else
	{
		advancewindow();
		switch (TREF(window_token))
		{
		case TK_COLON:
			/* Default device parms */
			plist = put_str((char *)empty_plist, SIZEOF(empty_plist));
			break;
		case TK_ATSIGN:
			/* Indirect for device parms */
			if (!indirection(&devpopr))
				return FALSE;
			indref = newtriple(OC_INDDEVPARMS);
			indref->operand[0] = devpopr;
			indref->operand[1] = put_ilit(IOP_OPEN_OK);
			inddevparms = TRUE;
			break;
		default:
			/* Literal device parms specified */
			if (!deviceparameters(&plist, IOP_OPEN_OK))
				return FALSE;
		}
	}
	/* Code generation for the optional timeout parm */
	is_timeout = FALSE;
	if (TK_COLON != TREF(window_token))
		timeout = put_ilit(NO_M_TIMEOUT);
	else
	{
		advancewindow();
		if (TK_COLON == TREF(window_token))
			timeout = put_ilit(NO_M_TIMEOUT);
		else
		{
			is_timeout = TRUE;
			if (EXPR_FAIL == expr(&timeout, MUMPS_INT))
				return FALSE;
		}
	}
	if (TK_COLON != TREF(window_token))
		mspace = put_lit((mval *)&literal_null);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&mspace, MUMPS_EXPR))
			return FALSE;
	}
	ref1 = newtriple(OC_OPEN);
	ref1->operand[0] = sopr;
	ref2 = newtriple(OC_PARAMETER);
	ref1->operand[1] = put_tref(ref2);
	ref2->operand[0] = !inddevparms ? plist : put_tref(indref);
	ref1 = newtriple(OC_PARAMETER);
	ref2->operand[1] = put_tref(ref1);
	ref1->operand[0] = timeout;
	ref2 = newtriple(OC_PARAMETER);
	ref1->operand[1] = put_tref(ref2);
	ref2->operand[0] = mspace;
	if (is_timeout)
		newtriple(OC_TIMTRU);
	return TRUE;
}
