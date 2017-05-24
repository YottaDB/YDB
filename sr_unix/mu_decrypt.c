	/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_facility.h"
#include "gtm_strings.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "fileinfo.h"
#include "filestruct.h"
#include "jnl.h"
#include "util.h"
#include "gtmio.h"
#include "gtmcrypt.h"
#include "mu_decrypt.h"

#define GC_DISPLAY_ERROR_AND_RETURN(MESSAGE, RC, ...)			\
{									\
	char *errptr;							\
									\
	util_out_print(MESSAGE, TRUE, __VA_ARGS__);			\
	if (0 < RC)							\
	{								\
		errptr = (char *)STRERROR(RC);				\
		util_out_print("System Error: !AZ", TRUE, errptr);	\
	}								\
	return RC;							\
}

/* Given a file (journal or database), the function extracts the buffer of the given length at the given offset and displays it on
 * the STDIN. Note that the offset and length should match the boundaries of a database block or a journal record at the encryption
 * time. In case of journal files, this offset could be obtained for every record using a detailed journal extract. That, however,
 * does not guarantee that the actual length of the content printed will be that of the data contained in the respective record due
 * to paddings inserted in journal files for proper alignment.
 *
 * NOTE: This utility does not work with encryption on-the-fly.
 */
int mu_decrypt(char *fname, int fname_len, uint4 off, uint4 len, char *type, int type_len)
{
	int		fd, save_errno, gtmcrypt_errno, i, status, iv_len;
	char		hash[GTMCRYPT_HASH_LEN], iv[GTM_MAX_IV_LEN], *iv_ptr, *buff, *buff_ptr;
	boolean_t	is_encrypted, is_journal;
	gtmcrypt_key_t	key_handle;
	jrec_prefix	*prefix;
	blk_hdr		*header;

	if (!STRNCASECMP_LIT(type, "JNL_NONLOG_IV") || !STRNCASECMP_LIT(type, "JNL_LOG_IV")
			|| !STRNCASECMP_LIT(type, "JNL_LOG_NO_IV") || !STRNCASECMP_LIT(type, "JNL_NONLOG_NO_IV"))
	{
		if (REAL_JNL_HDR_LEN > off)
			GC_DISPLAY_ERROR_AND_RETURN("Incorrect offset specified for file !AD with type !AD",
					-1, fname_len, fname, type_len, type);
		is_journal = TRUE;
	} else
	{
		assert(!STRNCASECMP_LIT(type, "DB_IV") || !STRNCASECMP_LIT(type, "DB_NO_IV"));
		if (SGMNT_HDR_LEN > off)
			GC_DISPLAY_ERROR_AND_RETURN("Incorrect offset specified for file !AD with type !AD",
					-1, fname_len, fname, type_len, type);
		is_journal = FALSE;
	}
	if (0 != (status = get_file_encr_hash(is_journal, fname, fname_len, &fd, hash, &is_encrypted)))
		return status;
	buff = (char *)malloc(len);
	buff_ptr = buff;
	LSEEKREAD(fd, off, buff_ptr, len, save_errno);
	if (0 != save_errno)
	{
		close(fd);
		free(buff);
		GC_DISPLAY_ERROR_AND_RETURN("Error reading from file !AD", save_errno, fname_len, fname);
	}
	if (is_encrypted)
	{
		INIT_PROC_ENCRYPTION(NULL, gtmcrypt_errno);
		if (is_journal)
		{
			GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(NULL, hash, 0, NULL, key_handle, gtmcrypt_errno);
		} else
		{
			GTMCRYPT_INIT_BOTH_CIPHER_CONTEXTS(NULL, hash, fname_len, fname, key_handle, gtmcrypt_errno);
		}
		if (0 != gtmcrypt_errno)
		{
			close(fd);
			free(buff);
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, fname_len, fname);
		}
	}
	if (!STRNCASECMP_LIT(type, "JNL_NONLOG_IV"))
	{	/* For non-logical records using non-null IVs the IV is the block header, so skip the prefix and other meta part. */
		prefix = (jrec_prefix *)buff_ptr;
		len = prefix->forwptr - FIXED_AIMG_RECLEN - SIZEOF(blk_hdr) - SIZEOF(jrec_suffix);
		buff_ptr += FIXED_AIMG_RECLEN;
		if (is_encrypted)
		{	/* Set up block-header-based IV. */
			iv_ptr = (char *)buff_ptr;
			iv_len = SIZEOF(blk_hdr);
		}
		buff_ptr += SIZEOF(blk_hdr);
	} else if (!STRNCASECMP_LIT(type, "JNL_LOG_IV"))
	{	/* For logical records using non-null IVs the IV is the forwptr field of the prefix, so first obtain that and then
		 * skip to the beginning of the data section.
		 */
		prefix = (jrec_prefix *)buff_ptr;
		len = prefix->forwptr - FIXED_UPD_RECLEN - SIZEOF(jrec_suffix);
		if (is_encrypted)
		{	/* Set up record-prefix-based IV. */
			PREPARE_LOGICAL_REC_IV(prefix->forwptr, iv);
			iv_ptr = (char *)iv;
			iv_len = SIZEOF(uint4) * 4;
		}
		buff_ptr += FIXED_UPD_RECLEN;
	} else if (!STRNCASECMP_LIT(type, "JNL_NONLOG_NO_IV"))
	{	/* For non-logical records using null IVs the IV is 16 zeroes (which is automatically filled in by the encryption
		 * plug-in), so skip the prefix and other meta part right away.
		 */
		prefix = (jrec_prefix *)buff_ptr;
		len = prefix->forwptr - FIXED_AIMG_RECLEN - SIZEOF(jrec_suffix);
		buff_ptr += FIXED_AIMG_RECLEN + SIZEOF(blk_hdr);
		iv_len = 0;
	} else if (!STRNCASECMP_LIT(type, "JNL_LOG_NO_IV"))
	{	/* For logical records using null IVs the IV is 16 zeroes (which is automatically filled in by the encryption
		 * plug-in), so skip the prefix and other meta part right away.
		 */
		prefix = (jrec_prefix *)buff_ptr;
		len = prefix->forwptr - FIXED_UPD_RECLEN - SIZEOF(jrec_suffix);
		iv_len = 0;
		buff_ptr += FIXED_UPD_RECLEN;
	} else if (!STRNCASECMP_LIT(type, "DB_IV"))
	{	/* For database blocks using non-null IVs the IV is the block header, so only skip to the records after setting up
		 * the IV.
		 */
		header = (blk_hdr *)buff_ptr;
		len = header->bsiz - SIZEOF(blk_hdr);
		if (is_encrypted)
		{	/* Set up block-header-based IV. */
			iv_ptr = (char *)buff_ptr;
			iv_len = SIZEOF(blk_hdr);
		}
		buff_ptr += SIZEOF(blk_hdr);
	} else
	{	/* For database blocks using null IVs the IV is 16 zeroes (which is automatically filled in by the encryption
		 * plug-in), so skip to the records right away.
		 */
		assert(!STRNCASECMP_LIT(type, "DB_NO_IV"));
		header = (blk_hdr *)buff_ptr;
		len = header->bsiz - SIZEOF(blk_hdr);
		iv_len = 0;
		buff_ptr += SIZEOF(blk_hdr);
	}
	if (is_encrypted)
	{
		GTMCRYPT_DECRYPT_WITH_IV(NULL, key_handle, buff_ptr, len, NULL, iv_ptr, iv_len, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			close(fd);
			free(buff);
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, fname_len, fname);
		}
	}
	for (i = 0; i < len; i++)
	{
		PRINTF("%c", buff_ptr[i]);
	}
	PRINTF("\n");
	FFLUSH(stdout);
	free(buff);
	close(fd);
	return SS_NORMAL;
}

int get_file_encr_hash(boolean_t is_journal, char *fname, int fname_len, int *fd, char *hash, boolean_t *is_encrypted)
{
	jnl_file_header	*jfh;
	sgmnt_data	*dfh;
	void		*header;
	int		save_errno, hdr_sz;
	uint4		status;

	OPENFILE(fname, O_RDONLY, *fd);	/* udi not available so OPENFILE_DB not used */
	if (-1 == *fd)
	{
		save_errno = errno;
		GC_DISPLAY_ERROR_AND_RETURN("Error accessing file !AD", save_errno, fname_len, fname);
	}
	hdr_sz = is_journal ? REAL_JNL_HDR_LEN : SGMNT_HDR_LEN;
	header = malloc(hdr_sz);
	LSEEKREAD(*fd, 0, header, hdr_sz, save_errno);
	if (0 != save_errno)
	{
		free(header);
		GC_DISPLAY_ERROR_AND_RETURN("Error reading file !AD", save_errno, fname_len, fname);
	}
	if (is_journal)
	{
		jfh = (jnl_file_header *)header;
		status = 0;
		CHECK_JNL_FILE_IS_USABLE(jfh, status, TRUE, fname_len, fname);
		if (0 != status)
		{	/* gtm_putmsg would have already been done by CHECK_JNL_FILE_IS_USABLE macro */
			free(header);
			return status;
		}
	} else
	{
		dfh = (sgmnt_data *)header;
		if (status = MEMCMP_LIT(dfh->label, GDS_LABEL_GENERIC))	/* Note: assignment! */
		{
			free(header);
			GC_DISPLAY_ERROR_AND_RETURN("Invalid database file !AD", status, fname_len, fname);
		}
	}
	if (is_journal)
	{
		memcpy(hash, jfh->encryption_hash, GTMCRYPT_HASH_LEN);
		*is_encrypted = IS_ENCRYPTED(jfh->is_encrypted);
	} else
	{
		memcpy(hash, dfh->encryption_hash, GTMCRYPT_HASH_LEN);
		*is_encrypted = IS_ENCRYPTED(dfh->is_encrypted);
	}
	free(header);
	return 0;
}
