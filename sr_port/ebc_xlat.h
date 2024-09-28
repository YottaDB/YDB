/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
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

#define TWO_BYTE_CHAR 2
#define THREE_BYTE_CHAR 3
typedef struct {
	unsigned char key[TWO_BYTE_CHAR];
	unsigned char value;
} w1252_match;
/* There is no four_byte_char conversion to W-1252, such cases would be handled in CHAR_NOT_FOUND */

void asc_to_ebc(unsigned char *estring_out, unsigned char *astring_in, int len);
void ebc_to_asc(unsigned char *astring_out, unsigned char *estring_in, int len);
boolean_t find_w1252(const w1252_match *table, size_t size, int *i, unsigned char **out_ptr ,char *in_ptr,
												unsigned short bytes_to_cmp);
int no_conversion(mstr *w1252_in);
int w1252_to_utf8(mstr *w1252_in);
int utf8_to_w1252(mstr *utf8_in);
int w1252_to_utf16(mstr *w1252_in);
int utf16_to_w1252(mstr *utf16_in);

#endif
