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
#ifndef GTCMD_H_INCLUDED
#define GTCMD_H_INCLUDED

void gtcmd_cst_init(cm_region_head *ptr);
cm_region_head *gtcmd_ini_reg(connection_struct *cnx);
void gtcmd_rundown(connection_struct *cnx, bool clean_exit);

#endif
