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
#ifndef __EBC_XLAT_H__
#define __EBC_XLAT_H__

void asc_to_ebc(unsigned char *estring_out, unsigned char *astring_in, int len);
void ebc_to_asc(unsigned char *astring_out, unsigned char *estring_in, int len);

#endif
