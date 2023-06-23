/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef EBC_XLAT_H_INCLUDED
#define EBC_XLAT_H_INCLUDED

void asc_to_ebc(unsigned char *estring_out, unsigned char *astring_in, int len);
void ebc_to_asc(unsigned char *astring_out, unsigned char *estring_in, int len);

#endif
