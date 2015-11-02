/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "util.h"
#include "gtmio.h"
#include "gtmcrypt.h"
#include "mu_decrypt.h"

#define WITHIN_PRINTABLE_RANGE(c)	(31 < (unsigned char)c && 127 > (unsigned char)c)
/* Given a file (journal or database), the function extracts the buffer of the given length at the given offset and displays it
 * on the STDIN. Note that, the offset and length should match the values at the encryption time. In case of journal files,
 * this offset could be obtained for every record using a detailed journal extract. */
int	mu_decrypt(char *fname, uint4 off, uint4 len)
{
#	ifdef GTM_CRYPT
	int				fd, n_len, save_errno, status, i;
	char				hash[GTMCRYPT_HASH_LEN], *buff;
	boolean_t			is_encrypted;
	gtmcrypt_key_t			key_handle;

	assert(fname);
	assert(STRLEN(fname));
	n_len = STRLEN(fname);
	GET_FD_HASH(fname, n_len, fd, hash, is_encrypted);
	buff = (char *)malloc(len);
	LSEEKREAD(fd, off, buff, len, save_errno);
	if (0 != save_errno)
	{
		close(fd);
		GC_DISPLAY_FILE_ERROR_AND_RETURN("Error reading from file !AD", fname, n_len, save_errno);
	}
	if (is_encrypted)
	{
		INIT_PROC_ENCRYPTION(status);
		GTMCRYPT_GETKEY(hash, key_handle, status);
		if (0 == status)
			GTMCRYPT_DECODE_FAST(key_handle, buff, len, NULL, status);
		if (0 != status)
		{
			close(fd);
			free(buff);
			GC_RTS_ERROR(status, fname);
		}
	}
	for (i = 0; i < len; i++)
	{
		if (WITHIN_PRINTABLE_RANGE(buff[i]))
			PRINTF("%c", buff[i]);
		else
			PRINTF("%c", '.');
	}
	free(buff);
	close(fd);
#endif
	return SS_NORMAL;

}
