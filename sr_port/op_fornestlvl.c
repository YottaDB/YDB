/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "lv_val.h"

/* This feeds compile time context (level) passed from m_for to op_saveputindx at run-time through a TREF
 * because  op_saveputindx function shares an argument format with others parsed by lvn
 */
void op_fornestlvl(uint4 level)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((0 < level) && (MAX_FOR_STACK >= level));
	TREF(for_nest_level) = level;
	return;
}
