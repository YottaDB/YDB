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

#ifndef __LA_IO_H__
#define  __LA_IO_H__

#include <rms.h>

 void bclose (struct FAB *f);

 void bdelete (struct FAB *f,struct XABPRO *x);

 int bcreat (
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 struct NAM *nam,
 struct XABPRO *xab,
 int n );

 int vcreat (
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 int n );

 int bopen(
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 struct XABFHC *x ,
 struct NAM *nam);

 int breopen(
 char *fn ,
 struct FAB *f ,
 struct RAB *r ,
 struct XABFHC *x ,
 struct NAM *nam);

 int bread (
 struct RAB *r ,
 char *p ,			/* user buffer address         */
 unsigned short w );		/* user buffer length in bytes */

 int bwrite (
 struct RAB *r ,
 char *p ,
 unsigned short w );

int vcreat(char *fn, struct FAB *f, struct RAB *r, int n);

#endif
