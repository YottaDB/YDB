/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef VIEW_INCLUDED
#define VIEW_INCLUDED

typedef struct
{
	unsigned char keyword[16];
	unsigned char parm;
	unsigned char keycode;
	unsigned char restype;
} viewtab_entry;

enum noisolation_type
{
	NOISOLATION_NULL,
	NOISOLATION_PLUS,
	NOISOLATION_MINUS
};

#define	NOISOLATION_INIT_ALLOC		8

typedef struct noisolation_element_struct
{
	struct gv_namehead_struct		*gvnh;
	struct noisolation_element_struct	*next;
} noisolation_element;

typedef struct noisolation_list_struct
{
	enum noisolation_type	type;
	noisolation_element	*gvnh_list;
} noisolation_list;

typedef union
{
	mident_fixed		ident;
	mval			*value;
	struct gd_region_struct *gv_ptr;
	noisolation_list	ni_list;
	mstr			str;
} viewparm;

#define VTP_NULL 1
#define VTP_VALUE 2
#define VTP_DBREGION 4
#define VTP_DBKEY 8
#define VTP_RTNAME 16
#define	VTP_DBKEYLIST 32
#define VTP_LVN 64

#define VIEWTAB(A,B,C,D) C

#define	IS_DOLLAR_VIEW_FALSE	FALSE
#define	IS_DOLLAR_VIEW_TRUE	TRUE

enum viewtab_keycode {
#include "viewtab.h"
};

viewtab_entry *viewkeys(mstr *v);
void view_arg_convert(viewtab_entry *vtp, int vtp_parm, mval *parm, viewparm *parmblk, boolean_t is_dollar_view);
void view_routines(mval *dst, mident_fixed *name);
void view_routines_checksum(mval *dst, mident_fixed *name);
int view_device(mstr *device_name, unsigned char *device, int device_len);	/* in zshow_devices.c */

#undef VIEWTAB

#endif /* VIEW_INCLUDED */
