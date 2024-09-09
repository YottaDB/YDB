/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "mdq.h"
#include "resolve_blocks.h"

GBLREF mline mline_root;

void resolve_blocks(void)
{
	triple *ref, *t1;
	mline *mlx, *mly;

	for (mlx = mline_root.child ; mlx ; mlx = mly)
	{
		if (mly = mlx->child)
		{
			if (mlx->sibling)
			{
				triple	*t2, *t3;

				ref = maketriple(OC_JMP);
				t1 = mlx->sibling->externalentry;
				/* We are about to jump from this line to its sibling line (which is a few M lines later
				 * because of intervening dotted DO lines). Check if the sibling line had a FALLINTOFLST
				 * error triple added before it (in line.c). If so, reset the jump target to that triple
				 * as this jump should result in the same fall-through error (YDB#1097).
				 */
				t2 = t1->exorder.bl;
				if (OC_RTERROR == t2->opcode)
				{
					t3 = t2->operand[0].oprval.tref;
					if ((ILIT_REF == t3->operand[0].oprclass)
							&& (ERR_FALLINTOFLST == t3->operand[0].oprval.ilit))
						t1 = t2;
				}
				ref->operand[0] = put_tjmp(t1);
			} else
				ref = maketriple(OC_RET);
			t1 = mly->externalentry;
			t1 = t1->exorder.bl;
			ref->src = t1->src;
			dqins(t1, exorder, ref);
		} else if ((mly = mlx->sibling) == 0)
		{
			for (mly = mlx->parent ; mly ; mly = mly->parent)
			{
				if (mly->sibling)
				{
					mly = mly->sibling;
					break;
				}
			}
			if (mly)
			{
				ref = maketriple(OC_RET);
				t1 = mly->externalentry->exorder.bl;
				ref->src = t1->src;
				dqins(t1, exorder, ref);
			}
		}
	}
}
