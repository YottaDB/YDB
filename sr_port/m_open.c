/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

GBLREF char	window_token;
LITREF mval	literal_null;

int m_open(void)
{
	int4		rval;
	static readonly unsigned char empty_plist[1] = { iop_eol };
	triple		tmpchain, *ref1, *ref2, *indref;
	oprtype		sopr, devpopr, plist, timeout, mspace;
	opctype		opcd;
	boolean_t	is_timeout, inddevparms;

	inddevparms = FALSE;
	if (!(rval = strexpr(&sopr)))
		return FALSE;
	if (TK_COLON != window_token)
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
		switch(window_token)
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
	if (TK_COLON != window_token)
		timeout = put_ilit(NO_M_TIMEOUT);
	else
	{
		advancewindow();
		if (TK_COLON == window_token)
			timeout = put_ilit(NO_M_TIMEOUT);
		else
		{
			is_timeout = TRUE;
			if (!intexpr(&timeout))
				return FALSE;
		}
	}
	if (TK_COLON != window_token)
		mspace = put_lit((mval *)&literal_null);
	else
	{
		advancewindow();
		if (!expr(&mspace))
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
