/****************************************************************
 *								*
 * Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gvusr.h"
#include "gvusr_queryget.h"

/* This file defines various stub functions for executables that link to functions related to the dba_usr access method
 * (unused DDP) but are guaranteed to never use/need them. Utilities like MUPIP/DSE/LKE etc. never need access to the
 * client-side functions of a DDP server so all such functions are included here.
 */

void gvusr_init(gd_region *reg, gd_region **creg, gv_key **ckey, gv_key **akey)
{
	return;
}

void gvusr_rundown(void)
{
	return;
}

int gvusr_data(void)
{
	return 1;
}

int gvusr_order(void)
{
	return 1;
}

int gvusr_query(mval *v)
{
	return 1;
}

int gvusr_reversequery(mval *v)
{
	return 1;
}

int gvusr_zprevious(void)
{
	return 1;
}


int gvusr_get(mval *v)
{
	v->mvtype = MV_STR;
	v->str.len = SIZEOF("TestData") - 1;
	v->str.addr = "TestData";
	return 1;
}

void gvusr_kill(bool do_subtree)
{
	return;
}

void gvusr_put(mval *v)
{
	return;
}


int gvusr_lock(uint4 lock_len, unsigned char *lock_key, gd_region *reg)
{	/* 0 indicates successful lock */
	return TRUE;
}

void gvusr_unlock(uint4 lock_len, unsigned char *lock_key, gd_region *reg)
{
	return;
}

boolean_t gvusr_queryget(mval *v)
{
	return 1;
}
