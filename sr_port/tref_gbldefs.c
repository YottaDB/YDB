/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* File containing all global variables that were formerly TREF() globals.
 * All of them will now have a "tref_" prefix in their name.
 * For example, what used to be TREF(transform) will now be the global variable "tref_transform".
 * This way it is clear this is a thread global even though it is actually a global variable.
 * The reason for the TREF -> global variable transition is a small performance benefit.
 * Saves 1 CPU cycle or more for every TREF() access in the code.
 */

GBLDEF	boolean_t		tref_transform;

