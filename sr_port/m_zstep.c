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
#include "compiler.h"
#include "opcode.h"
#include "zstep.h"
#include "toktyp.h"
#include "nametabtyp.h"
#include "mdq.h"
#include "advancewindow.h"
#include "cmd.h"
#include "namelook.h"

GBLREF char window_token;
GBLREF mval window_mval;
GBLREF mident window_ident;
GBLREF char director_token;
GBLREF short int source_column;
GBLREF short int last_source_column;
static readonly nametabent zstep_names[] =
{
	 { 1, "I"}, { 4, "INTO"}
	,{ 2,"OU" }, { 5,"OUTOF" }
	,{ 2,"OV" }, { 4,"OVER" }
};
static readonly unsigned char zstep_index[27] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2
	,2, 2, 6, 6, 6, 6, 6, 6, 6, 6, 6
	,6, 6, 6
};

static readonly char zstep_type[]={ ZSTEP_INTO, ZSTEP_INTO,
				    ZSTEP_OUTOF, ZSTEP_OUTOF,
				    ZSTEP_OVER, ZSTEP_OVER
};

int m_zstep(void)
{
	triple *head;
	char	type;
	int	x;
	oprtype action;
	opctype	op;
	error_def(ERR_INVZSTEP);
	error_def(ERR_ZSTEPARG);

	assert(zstep_index[26] == (sizeof(zstep_names)/sizeof(nametabent)));
	op = OC_ZSTEP;

	switch(window_token)
	{
	case TK_SPACE:
	case TK_EOL:
		type = ZSTEP_OVER;
		break;
	case TK_IDENT:
		if ((x = namelook(zstep_index,zstep_names,window_ident.c)) < 0)
		{
			stx_error(ERR_INVZSTEP);
			return FALSE;
		}
		type = zstep_type[x];
		advancewindow();
		break;
	default:
		stx_error(ERR_ZSTEPARG);
		return FALSE;
		break;
	}
	if (window_token == TK_COLON)
	{	advancewindow();
		if (!expr(&action))
			return FALSE;
		op = OC_ZSTEPACT;
	}
	head = maketriple(op);
	head->operand[0] = put_ilit(type);
	if (op == OC_ZSTEPACT)
		head->operand[1] = action;
	ins_triple(head);
	return TRUE;
}
