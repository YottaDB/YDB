/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

/* this module is just a set of stubs */

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
