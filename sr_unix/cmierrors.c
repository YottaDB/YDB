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
#include "merror.h"

LITDEF	err_msg cmierrors[] = {
	"DCNINPROG", "Attempt to initiate operation while disconnect was in progress", 0,
	"LNKNOTIDLE", "Attempt to initiate operation before previous operation completed", 0,
	"ASSERT", "Assert failed !AD line !UL", 3,
	"CMICHECK", "Internal CMI error--Report to Greystone Technology Corp", 0,
	"MEMORY", "Central memory exhausted", 0,
};

LITDEF	int CMI_DCNINPROG = 150634508;
LITDEF	int CMI_LNKNOTIDLE = 150634516;
LITDEF	int CMI_ASSERT = 150634522;
LITDEF	int CMI_CMICHECK = 150634532;
LITDEF	int CMI_MEMORY = 150634538;

LITDEF	err_ctl cmierrors_ctl = {
	250,
	"CMI",
	&cmierrors[0],
	5};
