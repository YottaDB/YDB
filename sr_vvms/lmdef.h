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

#ifndef __LMDEF_H__
#define __LMDEF_H__

int4 lm_mdl_nid(int4 *mdl, int4 *nid, int4 *csid);
void lm_edit(int4 kid, char *h, pak *p, int4 lo, int4 hi);
void lm_getnid(uint4 kid, int4 nid[], int4 sid[], int4 n);
void lm_listpak(pak *p);
void lm_putmsgu(int4 c, int4 fao[], short n);
int lm_maint (void);
int lm_register (void);
int lm_showcl (void);

#endif
