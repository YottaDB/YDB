/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "cmd.h"
#include "gtm_string.h"
#include "advancewindow.h"
#include "indir_enum.h"

error_def(ERR_LABELEXPECTED);
error_def(ERR_SPOREOL);

int m_trestart(void)
{
	triple	*ref, *calltrip, *no_label, *no_rtn, *oldchain, tmpchain, *obp = NULL;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	char *xpel_litu = "XPEL";
	char *xpel_litl = "xpel";
	if (TK_IDENT == TREF(window_token))
	{
		if ((strlen(xpel_litu) != (TREF(window_ident)).len) ||
			((!memcmp(xpel_litu, (TREF(window_ident)).addr, strlen(xpel_litu))) &&
			(!memcmp(xpel_litl, (TREF(window_ident)).addr, strlen(xpel_litl)))))
		{
			stx_error(ERR_SPOREOL);
			return FALSE;
		}
		advancewindow();
		if (TK_EQUAL != TREF(window_token))
		{
			stx_error(ERR_SPOREOL);
			return FALSE;
		}
		advancewindow();
		exorder_init(&tmpchain);
		oldchain = setcurtchain(&tmpchain);
		obp = oldchain->exorder.bl;
		/* Similar to m_zgoto use entryref() to create triples with text representation of routine name & label. */
		calltrip = entryref(OC_NOOP, OC_PARAMETER, (mint)indir_goto, FALSE, TRUE, TRUE);
		setcurtchain(oldchain);
		if (!calltrip)
			return FALSE;
	}
	if ((TK_EOL != TREF(window_token)) && (TK_SPACE != TREF(window_token)))
	{
		stx_error((TK_PLUS == TREF(window_token)) ? ERR_LABELEXPECTED : ERR_SPOREOL);
		return FALSE;
	}
	ref = newtriple(OC_TRESTART);
	ref->operand[0] = put_ilit(1);
	if (NULL != obp)
	{ /* If we have a XPEL parameter and we picked up the routine and label hook it up both ways. */
		ref->operand[1] = put_tref(tmpchain.exorder.bl);
		dqadd(obp, &tmpchain, exorder);
	} else
	{ /* Provide triple with null strings for the routine and label indicating we are not doing XPEL processing */
		no_label = newtriple(OC_PARAMETER);
		no_label->operand[0] = put_str("",0);
		no_label->operand[1] = put_ilit(0);
		no_rtn = newtriple(OC_PARAMETER);
		no_rtn->operand[0] = put_str("",0);
		no_rtn->operand[1] = put_tref(no_label);
		ref->operand[1] = put_tref(no_rtn);
	}
	return TRUE;
}
