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

#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "relqop.h"

/* shuffle shuffle sniff and snuffle */

void	shuffqtt (que_ent_ptr_t base1, que_ent_ptr_t base2)
{
	que_ent_ptr_t temp;

	temp = (que_ent_ptr_t)((uchar_ptr_t)base1 + base1->bl);
	base1->bl += temp->bl;
	((que_ent_ptr_t)((uchar_ptr_t)base1 + base1->bl))->fl += temp->fl;

	temp->fl = (int4)((uchar_ptr_t)base2 - (uchar_ptr_t)temp);
	temp->bl = temp->fl + base2->bl;
	base2->bl = -temp->fl;
	((que_ent_ptr_t)((uchar_ptr_t)temp + temp->bl))->fl = -temp->bl;

	return;
}

void	shuffqth (que_ent_ptr_t base1, que_ent_ptr_t base2)
{
	que_ent_ptr_t	temp;

	temp = (que_ent_ptr_t)((uchar_ptr_t)base1 + base1->bl);
	base1->bl += temp->bl;
	((que_ent_ptr_t)((uchar_ptr_t)base1 + base1->bl))->fl += temp->fl;

	temp->bl = (int4)((uchar_ptr_t)base2 - (uchar_ptr_t)temp);
	temp->fl = temp->bl + base2->fl;
	base2->fl = -temp->bl;
	((que_ent_ptr_t)((uchar_ptr_t)temp + temp->fl))->bl = -temp->fl;

	return;
}
