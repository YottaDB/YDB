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
				ref = maketriple(OC_JMP);
				ref->operand[0] = put_tjmp(mlx->sibling->externalentry);
			} else
			{	ref = maketriple(OC_RET);
			}
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
