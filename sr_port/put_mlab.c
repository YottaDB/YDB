/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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

/* Routine to add the given label mident to the local label table and return an
 * oprtype structure that defines the label.
 */
oprtype put_mlab(mident *lbl)
{
	oprtype a;

	a.oprclass = MLAB_REF;
	a.oprval.lab = get_mladdr(lbl);
	return a;
}
