/****************************************************************
 *								*
 * Copyright (c) 2009-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUPIP_CRYPT_INCLUDED
#define MUPIP_CRYPT_INCLUDED

int get_file_encr_hash(boolean_t is_journal, char *fname, int fname_len, int *fd, char *hash, boolean_t *is_encrypted);
int mu_decrypt(char *fname, int fname_len, uint4 offset, uint4 length, char *type, int type_len);

#endif /* MUPIP_CRYPT_INCLUDED */
