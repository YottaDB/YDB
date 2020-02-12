/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"

opctype bx_get_andor_opcode(opctype ref_opcode, opctype andor_opcode)
{
	opctype	new_andor_opcode;

	switch(ref_opcode)
	{
	case OC_COM:
		new_andor_opcode = ((OC_LASTOPCODE > andor_opcode)
					? (OC_LASTOPCODE + andor_opcode)
					: (andor_opcode - OC_LASTOPCODE));
		break;
	case OC_AND:
	case OC_OR:
	case OC_NAND:
	case OC_NOR:
		new_andor_opcode = ref_opcode;
		if (OC_LASTOPCODE <= andor_opcode)
			LOGICAL_NOT(new_andor_opcode);
		break;
	default:
		/* andor_opcode is already set to the desired value */
		new_andor_opcode = andor_opcode;
		break;
	}
	return new_andor_opcode;
}
