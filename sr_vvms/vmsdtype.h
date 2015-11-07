/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef VMSDTYPE_H_INCLUDED
#define VMSDTYPE_H_INCLUDED

#pragma member_alignment save
#pragma nomember_alignment

/* user-defined VMS 'conceptual' data type */
typedef struct
{	unsigned short int	buffer_length;
	unsigned short int	item_code;
	void			*buffer_address;
	void			*return_length_address; /* some system services expect this to be short * (eg. sys$getsyi), but
						         * some expect this to be a int4 * (eg. sys$getlki). Hence, we use void * */
} item_list_3;

#pragma member_alignment restore

#endif /* VMSDTYPE_H_INCLUDED */
