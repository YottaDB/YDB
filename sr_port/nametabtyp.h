/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef NAMETABTYP_H_INCLUDED
#define NAMETABTYP_H_INCLUDED

#define NAME_ENTRY_SZ	MAX_MIDENT_LEN	/* maximum length of ISV/device-param/TSTART-param/JOB-param names */
typedef struct {
	char len;
	char name[NAME_ENTRY_SZ];
} nametabent;

#endif /* NAMETABTYP_H_INCLUDED */
